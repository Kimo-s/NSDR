#include "XOPStandardHeaders.h"			// Include ANSI headers, Mac headers, IgorXOP.h, XOP.h and XOPSupport.h
#include "SXMreader.h"
#include "stdio.h"
#include "stdlib.h"
#include "gsl/gsl_integration.h"
#include "gsl/gsl_randist.h"
#include "gsl/gsl_vector.h"
#include "gsl/gsl_blas.h"
#include "gsl/gsl_multifit_nlinear.h"
#include "gsl/gsl_complex.h"
#include "gsl/gsl_complex_math.h"
#include "gsl/gsl_errno.h"
#define ERR(i) sqrt(gsl_matrix_get(covar, i, i))


static double integralLim = 30;
static double T = 1.5;

double F2(double t, void* params) {
    double Gamma = ((struct functionInput*)params)->Gamma;
    double Del = ((struct functionInput*)params)->Del;
    double V_0 = ((struct functionInput*)params)->V_0;
    double xdata = ((struct functionInput*)params)->xdata;
    double k = 0.086173;

    gsl_complex x;
    GSL_REAL(x) = fabs(t);
    GSL_IMAG(x) = Gamma;
    gsl_complex y;
    y = gsl_complex_sqrt(gsl_complex_sub_real(gsl_complex_pow_real(x,2), Del * Del));

    //double f = (GSL_REAL(gsl_complex_mul_real(gsl_complex_inverse(y), gsl_complex_abs(x))) * exp((t + xdata + V_0) / (k * T))) / (gsl_pow_2(1 + exp((t + xdata + V_0) / (k * T))) * (k * T));
    double f = (GSL_REAL(gsl_complex_mul(gsl_complex_inverse(y), x)) * exp((t + xdata + V_0) / (k * T))) / (gsl_pow_2(1 + exp((t + xdata + V_0) / (k * T))) * (k * T));

    return f;
}

double integrate_F2(struct functionInput input, int *error2) {

    gsl_integration_workspace* w = gsl_integration_workspace_alloc(2000);
    double error, Yi;
    size_t neval;
    gsl_function fu;
    fu.function = &F2;
    fu.params = &input;

    gsl_set_error_handler_off();
    int status;
    status = gsl_integration_qag(&fu, -integralLim, integralLim, 0, 1e-8, 1000, 6, w, &Yi, &error);

    /*gsl_integration_cquad_workspace* w = gsl_integration_cquad_workspace_alloc(100);
    status = gsl_integration_cquad(&fu, -integralLim, integralLim, 0, 1e-8, w, &Yi, &error, &neval);
    gsl_integration_cquad_workspace_free(w);*/

    gsl_integration_workspace_free(w);
    if (status) {
        XOPNotice("Error encountred with the integration.\n");
        DoUpdate();
        *error2 = -1;
    }
    return Yi;

}


int F(const gsl_vector* x, void* data, gsl_vector* f)
{
    size_t n = ((struct data*)data)->n;
    double* t = ((struct data*)data)->t;
    double* y = ((struct data*)data)->y;

    double Gamma = gsl_vector_get(x, 0);
    double Del = gsl_vector_get(x, 1);
    double A = gsl_vector_get(x, 2);
    double B = gsl_vector_get(x, 3);
    double C = gsl_vector_get(x, 4);
    double V_0 = gsl_vector_get(x, 5);

    for (int i = 0; i < n; i++)
    {
        struct functionInput input = { Gamma, Del, V_0, t[i]};
        int error;
        double Y = (C+B*(t[i]+V_0)+A*gsl_pow_2(t[i] + V_0))*integrate_F2(input, &error);
        if (error == -1) {
            XOPNotice("Error of integration sent to main function\n");
            DoUpdate();
            return GSL_FAILURE;
        }
        gsl_vector_set(f, i, Y - y[i]);
    }

    return GSL_SUCCESS;
}


void
callback(const size_t iter, void* params,
    const gsl_multifit_nlinear_workspace* w)
{
    gsl_vector* f = gsl_multifit_nlinear_residual(w);

    char message[130];
    sprintf(message, "Iter %zu: Gamma = %E, Del = %E, |f(x)| = %.4f\n", iter, gsl_vector_get(w->x, 0), gsl_vector_get(w->x, 1), gsl_blas_dnrm2(f));
    XOPNotice(message);
    DoUpdate();
}

extern "C" int
dynesFit(dynesFitParams * p) {
    char message[1000];

    if (p->wave == NULL) {
        return NOWAV;
    }
    integralLim = p->integralLimit;
    T = p->temperature;

    //check for the number of dimensions
    int result;
    int numDimensionsPtr; // Number of dimensions in the wave
    CountInt testdim[MAX_DIMENSIONS + 1]; // Array of dimension sizes

    if (result = MDGetWaveDimensions(p->wave, &numDimensionsPtr, testdim)) {
        return result;
    }
    if (numDimensionsPtr != 1) {
        return DIMENSION_MISMATCH;
    }

    const gsl_multifit_nlinear_type* T = gsl_multifit_nlinear_trust;
    gsl_multifit_nlinear_parameters params = gsl_multifit_nlinear_default_parameters();

    const size_t n = testdim[0];
    const size_t pr = 6;
    gsl_multifit_nlinear_workspace* w;

    // initial guess of the parameters 
    double x_init[pr] = {1.0, 1.0 , 1.0 , 1.0 , 1.0 , 1.0 };
    gsl_vector_view x = gsl_vector_view_array(x_init, pr);

    //data structure initilziation
    double* t = (double*)malloc(sizeof(double) * n);
    double* y = (double*)malloc(sizeof(double) * n);
    struct data d = { n, t, y };
    if (t == NULL || y == NULL) {
        return NOMEM;
    }

    // sets the function
    gsl_multifit_nlinear_fdf fdf;
    fdf.f = F;
    fdf.df = NULL;
    fdf.fvv = NULL;
    fdf.n = n;
    fdf.p = pr;
    fdf.params = &d;

    gsl_vector* f;
    int status, info;
    const double xtol = 1e-8;
    const double gtol = 1e-8;
    const double ftol = 0.0;


    double* ptr = (double*)WMNewPtr(n * sizeof(double));
    if (!ptr) {
        return NOMEM;
    }
    if (result = MDGetDPDataFromNumericWave(p->wave, ptr)) {
        WMDisposePtr((Ptr)ptr);
        return result;
    }
    double *dp = ptr;
    if (dp == NULL) {
        return NOMEM;
    }

    double ti;
    double dti;
    double x0;
    double topptr;
    double botptr;
    WaveScaling(p->wave, &dti, &x0, &topptr, &botptr);
    for (int i = 0; i < n; i++) {
        ti = x0 + dti * i;
        t[i] = ti;
        y[i] = *(dp + i);
        //sprintf(message, "x = %E, y = %E\n", t[i], y[i]);
        //XOPNotice(message);
    }
    WMDisposePtr((Ptr)ptr);
    
    w = gsl_multifit_nlinear_alloc(T, &params, n, pr);

    gsl_multifit_nlinear_winit(&x.vector, NULL, &fdf, w);

    double chisq0;
    f = gsl_multifit_nlinear_residual(w);
    gsl_blas_ddot(f, f, &chisq0);
    

    gsl_set_error_handler_off();
    status = gsl_multifit_nlinear_driver(200, xtol, gtol, ftol, /*callback*/ NULL , NULL, &info, w);
    if (status) {
        return NOMEM;
    }

    /*double lb[6]= { 0.03, 0.01, 0.03, 0.01, 0.003, 0.01 };
    double ub[6] = { 0.7, 0.2, 0.1, 1, 0.03, 0.3 };*/
    //gsl_set_error_handler_off();
    //for (int i = 0; i < 100; i++) {
    //    /*gsl_vector* parm = gsl_multifit_nlinear_position(w);
    //    for (int q = 0; q < pr; q++) {
    //        if (gsl_vector_get(parm, q) > ub[q] || gsl_vector_get(parm, q) < lb[q]) {
    //            gsl_vector_set(w->x, q, x_init[q]);
    //        }
    //    }*/
    //    status = gsl_multifit_nlinear_iterate(w);
    //    if (status) {
    //        return NOMEM;
    //    }
    //    gsl_vector* f = gsl_multifit_nlinear_residual(w);

    //    sprintf(message, "Iter %d: Gamma = %E, Del = %E, |f(x)| = %.4f\n", i, gsl_vector_get(w->x, 0), gsl_vector_get(w->x, 1), gsl_blas_dnrm2(f));
    //    XOPNotice(message);
    //    DoUpdate();
    //    status = gsl_multifit_nlinear_test(xtol, gtol, ftol, &info, w);
    //    if (status == GSL_SUCCESS) {
    //        break;
    //    }
    //}
    //XOPNotice("Fninished fitting\n");

    // Make a wave with the fitted parameters
    waveHndl fitplot;
    CountInt dimensionSizes[MAX_DIMENSIONS + 1];
    int err;

    MemClear(dimensionSizes, sizeof(dimensionSizes));
    dimensionSizes[ROWS] = n;

    char name[MAX_OBJ_NAME + 1];
    WaveName(p->wave, name);
    sprintf(message, "%s_DyneFitPlot", name);
    if (result = MDMakeWave(&fitplot, message, NULL, dimensionSizes, NT_FP64, 1)) {
        return result;
    }
    
    if (err = MDSetWaveScaling(fitplot, ROWS, &dti, &x0)) {
        return err;
    }

    gsl_vector* parm = gsl_multifit_nlinear_position(w);

    double Gamma = fabs(gsl_vector_get(parm, 0));
    double Del = fabs(gsl_vector_get(parm, 1));
    double A = gsl_vector_get(parm, 2);
    double B = gsl_vector_get(parm, 3);
    double C = gsl_vector_get(parm, 4);
    double V_0 = gsl_vector_get(parm, 5);

    gsl_matrix* covar = gsl_matrix_alloc(pr, pr);
    gsl_matrix* J;
    J = gsl_multifit_nlinear_jac(w);
    gsl_multifit_nlinear_covar(J, 0.0, covar);


    gsl_vector* final_residual = gsl_multifit_nlinear_residual(w);
    sprintf(message, "Gamma = %E +/- %E\n  Delta = %E +/- %E\n  A = %E +/- %E\n  B = %E +/- %E\n  C = %E +/- %E\n  V_0 = %E +/- %E\n  |f(x)| = %.5f\n", 
        Gamma, ERR(0), 
        Del, ERR(1), 
        A, ERR(2),
        B, ERR(3),
        C, ERR(4),
        V_0, ERR(5),
        gsl_blas_dnrm2(final_residual));
    XOPNotice(message);
    DoUpdate();

    CountInt dimensionSizes2[MAX_DIMENSIONS + 1];

    MemClear(dimensionSizes2, sizeof(dimensionSizes2));
    dimensionSizes2[ROWS] = 6;

    WaveName(p->wave, name);
    sprintf(message, "%s_DyneFitParameters", name);
    if (result = MDMakeWave(&p->result, message, NULL, dimensionSizes2, NT_FP64, 1)) {
        return result;
    }


    IndexInt indices[MAX_DIMENSIONS];
    double value[2];
    for (int i = 0; i < 6; i++) {
        indices[0] = i;
        value[0] = gsl_vector_get(parm, i);
        MDSetNumericWavePointValue(p->result, indices, value);
    }


   /* indices[0] = 0;
    value[0] = Gamma;
    MDSetNumericWavePointValue(p->wave, indices, value);

    indices[0] = 1;
    value[0] = Del;
    MDSetNumericWavePointValue(p->wave, indices, value);

    indices[0] = 2;
    value[0] = A;
    MDSetNumericWavePointValue(p->wave, indices, value);

    indices[0] = 3;
    value[0] = Gamma;
    MDSetNumericWavePointValue(p->wave, indices, value);

    indices[0] = 4;
    value[0] = Gamma;
    MDSetNumericWavePointValue(p->wave, indices, value);

    indices[0] = 5;
    value[0] = Gamma;
    MDSetNumericWavePointValue(p->wave, indices, value);*/


    float* waveptr = (float*)WaveData(fitplot);
    for (int i = 0; i < n; i++) {
        double ti = x0 + dti * i;
        struct functionInput input = { Gamma, Del , V_0, ti };
        double Y = (C + B * (ti + V_0) + A * gsl_pow_2(ti + V_0)) * integrate_F2(input, &status);
        indices[0] = i;
        value[0] = Y;
        MDSetNumericWavePointValue(fitplot, indices, value);
    }

    gsl_multifit_nlinear_free(w);
    free(t);
    free(y);

	return 0;
}


extern "C" int
dynesFitGrid(dynesFitParams* p) {
    if (!WaveType(p->wave) == NT_FP32 || !WaveType(p->wave) == NT_FP64 || p->wave == NULL) {
        return NT_INCOMPATIBLE;
    }

    int result;
    int numDimensionsPtr; // Number of dimensions in the wave
    CountInt dimensionSizes2[MAX_DIMENSIONS + 1]; // Array of dimension sizes

    if (result = MDGetWaveDimensions(p->wave, &numDimensionsPtr, dimensionSizes2)) {
        return result;
    }
    if (numDimensionsPtr != 3) {
        return WAVE_NOT_GRID;
    }

    DataFolderHandle root;
    GetRootDataFolder(0, &root);
    SetCurrentDataFolder(root);


    double dti;
    double x0;
    MDGetWaveScaling(p->wave, 0, &dti, &x0);

    double dy;
    double y0;
    MDGetWaveScaling(p->wave, 1, &dy, &y0);

    /*double dz;
    double z0;
    MDGetWaveScaling(p->wave, 2, &dz, &z0);*/


    char message[MAXCMDLEN + 1];
    waveHndl gammaWave;
    waveHndl deltaWave;
    CountInt dimensionSizes[MAX_DIMENSIONS + 1];
    int err;

    MemClear(dimensionSizes, sizeof(dimensionSizes));
    dimensionSizes[ROWS] = dimensionSizes2[ROWS];
    dimensionSizes[COLUMNS] = dimensionSizes2[COLUMNS];

    char name[MAX_OBJ_NAME + 1];
    WaveName(p->wave, name);
    sprintf(message, "%s_Gamma", name);
    if (result = MDMakeWave(&gammaWave, message, NULL, dimensionSizes, NT_FP32, 1)) {
        return result;
    }

    if (err = MDSetWaveScaling(gammaWave, ROWS, &dti, &x0)) {
        return err;
    }
    if (err = MDSetWaveScaling(gammaWave, COLUMNS, &dy, &y0)) {
        return err;
    }

    sprintf(message, "%s_Delta", name);
    if (result = MDMakeWave(&deltaWave, message, NULL, dimensionSizes, NT_FP32, 1)) {
        return result;
    }

    if (err = MDSetWaveScaling(deltaWave, ROWS, &dti, &x0)) {
        return err;
    }
    if (err = MDSetWaveScaling(deltaWave, COLUMNS, &dy, &y0)) {
        return err;
    }


    int rows = dimensionSizes2[ROWS];
    int columns = dimensionSizes2[COLUMNS];
    CountInt tempDims[MAX_DIMENSIONS + 1];
    MemClear(tempDims, sizeof(tempDims));
    tempDims[ROWS] = dimensionSizes2[LAYERS];
    IndexInt indices[MAX_DIMENSIONS];
    IndexInt tempindices[MAX_DIMENSIONS];
    double value[2];
   
    double dz;
    double z0;
    dz = (double)20 / (dimensionSizes2[LAYERS]-1);
    z0 = -10;


    DataFolderHandle folder;
    char foldername[MAX_OBJ_NAME + 1];
    sprintf(foldername, "%s_DynesPlots", name);
    if (NewDataFolder(root, foldername, &folder) != 0) {
        return CANT_FIND_FOLDER;
    }
    for (int x = 0; x < rows; x++) {
        for (int y = 0; y < columns; y++) {
            waveHndl temp;
            SetCurrentDataFolder(folder);

            sprintf(message, "%s_Dynestemp_%d_%d", name, x, y);
            if (result = MDMakeWave(&temp, message, NULL, tempDims, NT_FP32, 1)) {
                return result;
            }
            if (err = MDSetWaveScaling(temp, ROWS, &dz, &z0)) {
                return err;
            }

            indices[0] = x;
            indices[1] = y;
            indices[2] = 0;
            double toDivideby[2];
            MDGetNumericWavePointValue(p->wave, indices, toDivideby);
            for (int z = 0; z < dimensionSizes2[LAYERS]; z++) {
                indices[2] = z;
                tempindices[0] = z;
                MDGetNumericWavePointValue(p->wave, indices, value);
                value[0] = value[0] / toDivideby[0];
                MDSetNumericWavePointValue(temp, tempindices, value);
            }


            
            /*char tempname[MAX_OBJ_NAME + 1];
            WaveName(temp, name);

            DataFolderHandle currentFolder;
            char currentFoldername[MAXCMDLEN + 1];
            GetCurrentDataFolder(&currentFolder);
            GetDataFolderNameOrPath(currentFolder, ,currentFoldername);*/

            char command[MAXCMDLEN + 1];
            sprintf(command, "root:\'%s_DynesPlots\':\'%s\'", name, message);
            //XOPNotice(command);
            sprintf(message, "dynesFit(%s, %f, %f, %f, %f, %f, %f, %f, %f)", command, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, p->integralLimit, p->temperature);
            XOPCommand2(message, 0, 1);
            sprintf(message, "Fit of x=%d,y=%d done.\n", x, y);
            XOPNotice(message);
            DoUpdate();
            
            sprintf(message, "%s_Dynestemp_%d_%d_DyneFitParameters", name, x, y);

            waveHndl params = FetchWaveFromDataFolder(folder,message);
            if (params == NULL) {
                return NOMEM;
            }
            /*if (WaveType(params) != NT_FP32) {
                return NOWAV;
            }*/

            tempindices[0] = 0;
            MDGetNumericWavePointValue(params, tempindices, value);
            MDSetNumericWavePointValue(gammaWave, indices, value);

            tempindices[0] = 1;
            MDGetNumericWavePointValue(params, tempindices, value);
            MDSetNumericWavePointValue(deltaWave, indices, value);

        }
    }
    SetCurrentDataFolder(root);
    p->result = NULL;
    return 0;
}
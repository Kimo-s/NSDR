#include "XOPStandardHeaders.h"			// Include ANSI headers, Mac headers, IgorXOP.h, XOP.h and XOPSupport.h
#include "SXMreader.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "vector"
#include "iostream"
#include "filesystem"
#include <iostream>
#include <fstream>
#include <filesystem>
namespace fs = std::filesystem;

extern "C" int
readSXMFile(readSXMFileParams * p)
{
	char filepath[MAX_OBJ_NAME + 1];
	int width;
	int height;
	float widthRange;
	float heightRange;
	float widthOffset;
	float heightOffset;
	int scan_direction = 0; // 0 for down and 1 for up
	FILE* fp;
	int err;
	char buff[1000];
	char message[1000];

	GetCStringFromHandle(p->fp, filepath, MAX_OBJ_NAME);
	if (fopen_s(&fp, filepath, "rb") != 0) {
		return FILE_NOT_FOUND;
	}

	if (fp == NULL) {
		return FILE_NOT_FOUND;
	}


	// Read the image resolution from the SXM file header
	int found_headers = 0;
	std::vector<std::string> channelNames;
	std::vector<int> channelDirections;
	int totalChannelDirections = 0;
 	while(found_headers != 5) {
		fscanf_s(fp, "%s", buff, sizeof(buff));
		/*sprintf(message, "%s\n", buff);
		XOPNotice(message);*/
		if (strcmp(buff, ":SCAN_PIXELS:") == 0) {
			fscanf_s(fp, "%d", &width, sizeof(width));
			fscanf_s(fp, "%d", &height, sizeof(height));
			found_headers++;
		}
		else if(strcmp(buff, ":SCAN_RANGE:") == 0){
			fscanf_s(fp, "%E", &widthRange, sizeof(widthRange));
			fscanf_s(fp, "%E", &heightRange, sizeof(heightRange));
			found_headers++;
		}
		else if (strcmp(buff, ":SCAN_DIR:") == 0) {
			fscanf_s(fp, "%s", buff, sizeof(buff));
			if (strcmp(buff, "up") == 0) {
				scan_direction = 1;
			}
			found_headers++;
		}
		else if (strcmp(buff, ":SCAN_OFFSET:") == 0) {
			fscanf_s(fp, "%E", &widthOffset, sizeof(widthOffset));
			fscanf_s(fp, "%E", &heightOffset, sizeof(heightOffset));
			/*sprintf(message, "Widthoffset = %e, heightoffset = %e\n", widthOffset, heightOffset);
			XOPNotice(message);*/
			found_headers++;
		}
		else if (strcmp(buff, ":DATA_INFO:") == 0) {
			fgets(buff, sizeof(buff), fp);
			fgets(buff, sizeof(buff), fp);
			fgets(buff, sizeof(buff), fp);
			while (strcmp(buff, "\n") != 0) {
				std::string temp(buff);
				if (temp.find("both") == std::string::npos) {
					channelDirections.push_back(1);
					totalChannelDirections += 1;
				}
				else {
					channelDirections.push_back(2);
					totalChannelDirections += 2;
				}
				// Read to the channel name
				char* nextToken;
				char* token = strtok_s(buff, " ,\t\n", &nextToken);
				token = strtok_s(NULL, " ,\t\n", &nextToken);

				channelNames.push_back(std::string(token));

				/*sprintf(message, "Found Name: %s, Number of copies: %d\n", channelNames.back().c_str(), channelDirections.back());
				XOPNotice(message);*/
				fgets(buff, sizeof(buff), fp);
			}
			found_headers++;
		}

	}


	// Read until the start of the binary data
	while (1) {
		fread(buff, 1, 1, fp);
		if ((int)buff[0] == 0x1a) {
			fread(buff, 1, 1, fp);
			if ((int)buff[0] == 0x04) {
				break;
			}
		}
	}

	int result;
	char waveName[MAX_OBJ_NAME+1];
	CountInt dimensionSizes[MAX_DIMENSIONS+1];
	DataFolderHandle folder;
	DataFolderHandle parentFolder;
	std::vector <float*> wavePtrs(totalChannelDirections);

	GetCStringFromHandle(p->waveName, waveName, MAX_OBJ_NAME);
	if (GetCurrentDataFolder(&parentFolder)) {
		return CANT_FIND_FOLDER;
	}

	// Check if name exists already for the folder and rename accordingly
	int n = 0;
	while (1) {
		if (n == 0) {
			sprintf(buff, "Variable temp = DataFolderExists(\"%s\")", waveName);
		}
		else {
			sprintf(buff, "Variable temp = DataFolderExists(\"%s (%d)\")", waveName, n);
		}
		XOPCommand2(buff, 0, 0);
		double real;
		double img;    //ignore
		FetchNumVar("temp", &real, &img);
		XOPCommand2("KillVariables/Z temp", 0, 0);
		if (real == 0) {
			if (n == 0) {
				break;
			}
			sprintf(waveName, "%s (%d)", waveName, n);
			break;
		}
		n++;
	}

	if (NewDataFolder(parentFolder, waveName, &folder) != 0) {
		return CANT_FIND_FOLDER;
	}
	
	MemClear(dimensionSizes, sizeof(dimensionSizes));
	dimensionSizes[ROWS] = width;
	dimensionSizes[COLUMNS] = height;

	int q = 0;
	char name[MAX_OBJ_NAME + 1];
	for (int i = 0; i < totalChannelDirections; i++) {

		bool flip = true;
		if (channelDirections.at(q) > 0) {
			sprintf(name, "%s__%s__%d", waveName, channelNames.at(q).c_str(), channelDirections.at(q));
			if (channelDirections.at(q) == 2) {
				flip = false;
			}
			channelDirections.at(q) = channelDirections.at(q) - 1;
		}
		else {
			q++;
			sprintf(name, "%s__%s__%d", waveName, channelNames.at(q).c_str(), channelDirections.at(q));
			channelDirections.at(q) = channelDirections.at(q) - 1;
		}

		waveHndl test;

		if (result = MDMakeWave(&test, name, folder, dimensionSizes, NT_FP32, 1)) {
			return result;
		}

		/*float* ptr = (float*)WaveData(test);*/
		float ex;
		IndexInt indices[MAX_DIMENSIONS];
		double value[2];
		if (scan_direction == 0) {
			if (flip) {
				for (int i = 0; i < height; i++) {
					for (int y = 0; y < width; y++) {
						fread((void*)(&ex), sizeof(float), 1, fp);
						indices[0] = width - 1 - y;
						indices[1] = i;
						value[0] = ReverseFloat(ex);
						MDSetNumericWavePointValue(test, indices, value);
					}
					//*(ptr + i) = ReverseFloat(ex);
				}
			}
			else {
				for (int i = 0; i < height; i++) {
					for (int y = 0; y < width; y++) {
						fread((void*)(&ex), sizeof(float), 1, fp);
						indices[0] = y;
						indices[1] = i;
						value[0] = ReverseFloat(ex);
						MDSetNumericWavePointValue(test, indices, value);
					}
					//*(ptr + i) = ReverseFloat(ex);
				}
			}
		}
		else {
			for (int i = 0; i < height; i++) {
				for (int y = 0; y < width; y++) {
					fread((void*)(&ex), sizeof(float), 1, fp);
					indices[0] = width - 1 - y;
					indices[1] = i;
					value[0] = ReverseFloat(ex);
					MDSetNumericWavePointValue(test, indices, value);
				}
			}
		}

		/*sprintf(message, "Widthoffset = %e, heightoffset = %e, heightRange = %e, widthRange = %e\n", widthOffset, heightOffset, heightRange, widthRange);
		XOPNotice(message);*/

		double offset = widthOffset - widthRange / 2;
		double delta = widthRange/(width-1);
		if (err = MDSetWaveScaling(test, ROWS, &delta, &offset)) {
			return err;
		}
		offset = heightOffset - heightRange / 2;
		delta = heightRange/(height-1);
		if (err = MDSetWaveScaling(test, COLUMNS, &delta, &offset)) {
			return err;
		}
		p->result = test;

	}



	fclose(fp);
	return 0;
}


//extern "C" int
//readDATFile(readSXMFileParams * p) {
//	char filepath[MAX_OBJ_NAME + 1];
//	FILE* fp;
//	char buff[10000];
//
//	float startOfBias;
//	float endOfBias;
//
//	GetCStringFromHandle(p->fp, filepath, MAX_OBJ_NAME);
//	if (fopen_s(&fp, filepath, "r") != 0) {
//		return FILE_NOT_FOUND;
//	}
//
//	//checks if the file is a dat file
//	if (strcmp(fs::path(filepath).extension().string().c_str(), ".dat") != 0) {
//		sprintf(buff, "Error: A none DAT file was found (the file path: %s)\n", fs::path(filepath).string().c_str());
//		XOPNotice(buff);
//		return FILE_NOT_FOUND;
//	}
//
//	int found_headers = 0;
//	char message[1000];
//	while (found_headers != 3) {
//		fscanf_s(fp, "%s", buff, sizeof(buff));
//		if (strcmp(buff, "[DATA]") == 0) {
//			fgets(buff, sizeof(buff), fp);
//			fgets(buff, sizeof(buff), fp);
//			found_headers++;
//		}
//		else if (strcmp(buff, "Sweep") == 0 || strcmp(buff,"Spectroscopy>Sweep") == 0) {
//			fscanf_s(fp, "%s", buff, sizeof(buff));
//			if (strcmp(buff, "Start") == 0) {
//				fscanf_s(fp, "%s", buff, sizeof(buff));
//				if (strcmp(buff, "(V)") == 0) {
//					fscanf_s(fp, "%E", &startOfBias);
//				}
//				else {
//					startOfBias = atof(buff);
//				}
//				//sprintf(message, "startOfBias = %e\n", startOfBias);
//				//XOPNotice(message);
//				found_headers++;
//			}
//			else if (strcmp(buff, "End") == 0){
//				fscanf_s(fp, "%s", buff, sizeof(buff));
//				if (strcmp(buff, "(V)") == 0) {
//					fscanf_s(fp, "%E", &endOfBias);
//				}
//				else {
//					endOfBias = atof(buff);
//				}
//				//sprintf(message, "endOfBias = %e\n", endOfBias);
//				//XOPNotice(message);
//				found_headers++;
//			}
//		}
//
//	}
//
//	int graphs = 0;
//	char* nextToken;
//	char* token = strtok_s(buff, " ,\t\n", &nextToken);
//	fgets(buff, sizeof(buff), fp);
//	while (token != NULL) {
//		token = strtok_s(NULL, " ,\t\n", &nextToken);
//		graphs++;
//	}
//	graphs = graphs - 1;
//
//
//	int points = 1;
//	while (fgets(buff, sizeof(buff), fp) != NULL) {
//		points++;
//	}
//	rewind(fp);
//
//	char labelLine[10000];
//	while (1) {
//		fscanf_s(fp, "%s", buff, sizeof(buff));
//		if (strcmp(buff, "[DATA]") == 0) {
//			fgets(buff, sizeof(buff), fp);
//			fgets(labelLine, sizeof(labelLine), fp);
//			break;
//		}
//	}
//	std::vector <waveHndl> wavePtrs(graphs);
//	std::vector <std::string> labels;
//	std::string labelLineString(labelLine);
//
//	labelLineString.erase(0, labelLineString.find(")") + 1);
//	size_t nextBracketPos;
//	while (1) {
//		nextBracketPos = labelLineString.find(")");
//		if (nextBracketPos == std::string::npos) {
//			break;
//		}
//		labels.push_back(labelLineString.substr(labelLineString.find_first_not_of(" \n\r\t\f\v"), nextBracketPos - 3));
//		size_t whitespacepos;
//		while (1) {
//			whitespacepos = labels.at(labels.size() - 1).find_first_of(" []");
//			if (whitespacepos == std::string::npos) {
//				break;
//			}
//			labels.at(labels.size() - 1).replace(whitespacepos, 1, 1, '_');
//		}
//		labelLineString.erase(0, nextBracketPos + 1);
//	}
//
//
//
//	
//
//	int result;
//	char waveName[MAX_OBJ_NAME + 1];
//	CountInt dimensionSizes[MAX_DIMENSIONS + 1];
//	int err;
//	double offset = startOfBias;
//	double delta = (endOfBias - startOfBias)/(points-1);
//	DataFolderHandle folder;
//	DataFolderHandle checker;
//	DataFolderHandle parentFolder;
//
//	MemClear(dimensionSizes, sizeof(dimensionSizes));
//	dimensionSizes[ROWS] = points;
//	GetCStringFromHandle(p->waveName, waveName, MAX_OBJ_NAME);
//	if (GetCurrentDataFolder(&parentFolder)) {
//		return CANT_FIND_FOLDER;
//	}
//	/*if (GetNamedDataFolder(parentFolder, waveName, &checker) != 0) {
//		return CANT_FIND_FOLDER;
//	}*/
//	
//	//if (checker == NULL) {
//		if (NewDataFolder(parentFolder, waveName, &folder) != 0) {
//			return CANT_FIND_FOLDER;
//		}
//	//}
//	/*else {
//		return CANT_FIND_FOLDER;
//	}*/
//
//
//
//	char name[MAX_OBJ_NAME + 1];
//	for (int i = 0; i < graphs-1; i++) {
//		waveHndl temp;
//		sprintf(name, "%s", labels.at(i).c_str());
//		if (result = MDMakeWave(&temp, name, folder, dimensionSizes, NT_FP32, 1)) {
//			return result;
//		}
//		wavePtrs.at(i) = temp; //(float*)WaveData(p->result);
//		if (err = MDSetWaveScaling(temp, ROWS, &delta, &offset)) {
//			return err;
//		}
//	}
//	p->result = wavePtrs.at(0);
//
//
//
//	float datatemp;
//	int y = 0;
//	while (fgets(buff, sizeof(buff), fp) != NULL) {
//		char* nextToken;
//		char* token = strtok_s(buff, " ,\t\n", &nextToken);
//		for(int x = 0; x < graphs-1;x++) {
//			token = strtok_s(NULL, " ,\t\n", &nextToken);
//			sscanf_s(token, "%E", &datatemp, (unsigned)sizeof(float));
//
//			IndexInt cords[1];
//			cords[0] = y;
//			double value[2];
//			value[0] = datatemp;
//			MDSetNumericWavePointValue(wavePtrs.at(x), cords, value);
//		}
//		y++;
//	}
//
//	fclose(fp);
//
//
//	return 0;
//}

extern "C" int
readfile(int type) {
	char filepath[MAX_PATH_LEN + 1];
	char message[MAX_PATH_LEN + 100];

	if (type == SXM) {
		if (XOPOpenFileDialog2(0, "Open SXM file", "SXM files:.sxm;", NULL, "", "", NULL, filepath) != 0) {
			return 0;
		}
	}
	else if (type == DAT) {
		if (XOPOpenFileDialog2(0, "Open DAT file", "DAT files:.dat;", NULL, "", "", NULL, filepath) != 0) {
			return 0;
		}
	}
	else if (type == FILE_3DS) {
		if (XOPOpenFileDialog2(0, "Open DAT file", "DAT files:.3ds;", NULL, "", "", NULL, filepath) != 0) {
			return 0;
		}
	}

	fs::path file = fs::path(filepath);
	std::string thePath = file.string();
	std::replace(thePath.begin(), thePath.end(), '\\', '/');

	if (type == 1) {
		sprintf(message, "readSXMFile(\"%s\",\"%s\")", thePath.c_str(), file.filename().string().c_str());
		XOPCommand(message);
	}
	else if (type == 2) {
		sprintf(message, "readDATFile(\"%s\",\"%s\")", thePath.c_str(), file.filename().string().c_str());
		XOPCommand(message);
	}
	else if (type == 3) {
		sprintf(message, "read3ds(\"%s\",\"%s\")", thePath.c_str(), file.filename().string().c_str());
		XOPCommand(message);
	}

	return 0;
}

extern "C" int
readDATFolder(readSXMFileParams * p) {

	char folder[MAX_OBJ_NAME + 1];
	char waveName[MAX_OBJ_NAME + 1];
	char folder2[MAX_OBJ_NAME + 1];
	char waveName2[MAX_OBJ_NAME + 1];
	GetCStringFromHandle(p->fp, folder, MAX_OBJ_NAME);
	GetCStringFromHandle(p->waveName, waveName, MAX_OBJ_NAME);


	int i = 0;
	for (const auto& entry : fs::directory_iterator(folder)) {
		sprintf(folder2, "%s", entry.path().string().c_str());
		sprintf(waveName2, "%s(%d)", waveName, i);
		PutCStringInHandle(folder2, p->fp);
		PutCStringInHandle(waveName2, p->waveName);
		readDATFile(p);
		i++;
	}

	return 0;
}

extern "C" int
plotDemodX(readSXMFileParams * p) {
	char folder[MAX_OBJ_NAME + 1];
	char buff[3000];
	float startOfBias, endOfBias;
	float x1, x2, y1, y2;

	int height = 0;
	int width = 0;
	int foundPoints = 0;
	std::vector <std::vector<float>> data;
	GetCStringFromHandle(p->fp, folder, MAX_OBJ_NAME);
	for (const auto& entry : fs::directory_iterator(folder)) {

		FILE* fp;
		if (fopen_s(&fp, entry.path().string().c_str(), "r") != 0) {
			return FILE_NOT_FOUND;
		}

		if (fp == NULL) {
			return FILE_NOT_FOUND;
		}

		int found_headers = 0;
		while (found_headers != 3) {
			fscanf_s(fp, "%s", buff, sizeof(buff));
			if (strcmp(buff, "[DATA]") == 0) {
				fgets(buff, sizeof(buff), fp);
				fgets(buff, sizeof(buff), fp);
				found_headers++;
			}
			else if (strcmp(buff, "Start") == 0) {
				fscanf_s(fp, "%E", &startOfBias, sizeof(startOfBias));
				found_headers++;
			}
			else if (strcmp(buff, "End") == 0) {
				fscanf_s(fp, "%E", &endOfBias, sizeof(endOfBias));
				found_headers++;
			}
			else if (foundPoints == 0 && strcmp(buff, "X") == 0) {
				fscanf_s(fp, "%s", buff, sizeof(buff));
				fscanf_s(fp, "%E", &x1, sizeof(x1));

				fscanf_s(fp, "%s", buff, sizeof(buff));
				fscanf_s(fp, "%s", buff, sizeof(buff));
				fscanf_s(fp, "%E", &y1, sizeof(y1));
			}
			else if (foundPoints == 1 && strcmp(buff, "X") == 0) {
				fscanf_s(fp, "%s", buff, sizeof(buff));
				fscanf_s(fp, "%E", &x2, sizeof(x2));

				fscanf_s(fp, "%s", buff, sizeof(buff));
				fscanf_s(fp, "%s", buff, sizeof(buff));
				fscanf_s(fp, "%E", &y2, sizeof(y2));
			}
		}

		if (foundPoints != 1) {
			int points = 0;
			while (fgets(buff, sizeof(buff), fp) != NULL) {
				points++;
			}
			rewind(fp);
			while (1) {
				fscanf_s(fp, "%s", buff, sizeof(buff));
				if (strcmp(buff, "[DATA]") == 0) {
					fgets(buff, sizeof(buff), fp);
					fgets(buff, sizeof(buff), fp);
					break;
				}
			}
			width = points;
		}

		float datatemp;
		int i = 0;
		std::vector<float> col(width);
		while (fgets(buff, sizeof(buff), fp) != NULL) {
			char* nextToken;
			char* token = strtok_s(buff, " ,\t\n", &nextToken);
			token = strtok_s(NULL, " ,\t\n", &nextToken);
			token = strtok_s(NULL, " ,\t\n", &nextToken);
			sscanf_s(token, "%E", &datatemp, (unsigned)sizeof(float));
			col.at(i) = datatemp;
			i++;
		}
		data.push_back(col);
		col.empty();

		fclose(fp);

		height++;
		foundPoints++;
	}
	

	int result;
	char waveName[MAX_OBJ_NAME + 1];
	CountInt dimensionSizes[MAX_DIMENSIONS + 1];

	GetCStringFromHandle(p->waveName, waveName, MAX_OBJ_NAME);

	MemClear(dimensionSizes, sizeof(dimensionSizes));
	dimensionSizes[ROWS] = width;
	dimensionSizes[COLUMNS] = height;

	if (result = MDMakeWave(&p->result, waveName, NULL, dimensionSizes, NT_FP32, 1)) {
		return result;
	}

	double offset = startOfBias;
	double delta = (endOfBias - startOfBias) / (width - 1);
	int err;
	if (err = MDSetWaveScaling(p->result, ROWS, &delta, &offset)) {
		return err;
	}
	offset = 0;
	delta = sqrt((x1 - x2)* (x1 - x2) + (y1 - y2)* (y1 - y2));
	if (err = MDSetWaveScaling(p->result, COLUMNS, &delta, &offset)) {
		return err;
	}

	float* waveptr = (float*)WaveData(p->result);
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			*(waveptr + (x + y * width)) = data.at(y).at(x);
		}
	}

	return 0;
}

extern "C" int
plotDemodY(readSXMFileParams * p) {
	return 0;
}

static XOPIORecResult
RegisterFunction()
{
	int funcIndex;

	funcIndex = (int)GetXOPItem(0);	/* which function invoked ? */
	switch (funcIndex) {
		case 0:
			return (XOPIORecResult)readSXMFile;
			break;
		case 1:
			return (XOPIORecResult)readDATFile;
			break;
		case 2:
			return (XOPIORecResult)readDATFolder;
			break;
		case 3:
			return (XOPIORecResult)plotDemodX;
			break;
		case 4:
			return (XOPIORecResult)plotDemodY;
			break;
		case 5:
			return (XOPIORecResult)dynesFit;
			break;
		case 6:
			return (XOPIORecResult)findpeaks;
			break;
		case 7: 
			return (XOPIORecResult)dynesFitGrid;
			break;
		case 8:
			return (XOPIORecResult)removebackground;
			break;
		case 9:
			return (XOPIORecResult)read3ds;
			break;
	}
	return 0;
}

/*	DoFunction()

	This will actually never be called because all of the functions use the direct method.
	It would be called if a function used the message method. See the XOP manual for
	a discussion of direct versus message XFUNCs.
*/
static int
DoFunction()
{
	int funcIndex;
	void *p;				/* pointer to structure containing function parameters and result */
	int err;

	funcIndex = (int)GetXOPItem(0);	/* which function invoked ? */
	p = (void*)GetXOPItem(1);		/* get pointer to params/result */
	switch (funcIndex) {
		
	}
	return(0);
}

/*	XOPEntry()

	This is the entry point from the host application to the XOP for all messages after the
	INIT message.
*/

static XOPIORecResult
XOPMenuItem() {
	int resourceMenuID, actualMenuID;
	int resourceItemNumber, actuaItemNumber;
	char message[MAX_PATH_LEN + 20];


	actualMenuID = (int)GetXOPItem(0);
	actuaItemNumber = (int)GetXOPItem(1);


	resourceItemNumber = ActualToResourceItem(actualMenuID, actuaItemNumber);
	resourceMenuID = ActualToResourceMenuID(actualMenuID);
	int selecteditem = 0;
	if (resourceMenuID != 0) {
		selecteditem = resourceMenuID;
	}

	switch (selecteditem)
	{
	//"load file" submenu
	case 101:
		// 1 for sxm files and 2 for dat files
		switch (actuaItemNumber) {
		case 1:
			return readfile(1);
			break;
		case 2:
			return readfile(2);
			break;
		case 3:
			return readfile(3);
			break;
		}
	//main menu case
	case 100:
		//"plot demod x" manu item
		if (actuaItemNumber == 2) {
			char filePathOut[MAX_PATH_LEN + 1];

			XOPCommand2("NewPath/O path", 1, 0);
			Handle msgH = WMNewHandle(sizeof(Handle));
			HistoryFetchText(NULL, NULL, &msgH);
			GetCStringFromHandle(msgH, message, 200);

			std::string msg(message);
			int firstdel = msg.find("\"");
			int lastdel = msg.find_last_of("\"");
			msg = msg.substr(firstdel + 1, lastdel - firstdel - 2);
			if (msg.length() <= 0) {
				return 0;
			}
			GetNativePath(msg.c_str(), filePathOut);

			fs::path file = fs::path(filePathOut);
			auto i = file.end();
			i--;
			sprintf(message, "plotDemodX(\"%s\",\"%s\")", filePathOut, (*i).string().c_str());
			XOPCommand2(message, 1, 1);
		}
		//"DyneFit" menu item
		else if (actuaItemNumber == 3) {
			//XOPNotice("Under development\n");
			
			XOPCommand2("String/G temp = GetBrowserSelection(0)", 1, 0);
			FetchStrVar("temp", message);
			XOPCommand2("KillStrings/Z temp", 1, 0);
			if (message == NULL || strcmp(message, "") == 0) {
				return NOWAV;
			}

			XOPNotice(message);

		}
		break;
	}

	return 0;
}

extern "C" void
XOPEntry(void)
{	
	XOPIORecResult result = 0;

	switch (GetXOPMessage()) {
		case FUNCTION:								/* our external function being invoked ? */
			result = DoFunction();
			break;

		case FUNCADDRS:
			//XOPNotice("called from funcaddrs\n");
			result = RegisterFunction();
			break;

		case MENUITEM:
			//XOPNotice("called from MENUITEM\n");
			result = XOPMenuItem();
			break;
	}
	SetXOPResult(result);
}

/*	XOPMain(ioRecHandle)

	This is the initial entry point at which the host application calls XOP.
	The message sent by the host must be INIT.
	
	XOPMain does any necessary initialization and then sets the XOPEntry field of the
	ioRecHandle to the address to be called for future messages.
*/

HOST_IMPORT int
XOPMain(IORecHandle ioRecHandle)
{	
	XOPInit(ioRecHandle);					// Do standard XOP initialization
	SetXOPEntry(XOPEntry);					// Set entry point for future calls
	
	if (igorVersion < 800) {				// XOP Toolkit 8.00 or later requires Igor Pro 8.00 or later
		SetXOPResult(OLD_IGOR);
		return EXIT_FAILURE;
	}
	
	SetXOPResult(0);
	return EXIT_SUCCESS;
}

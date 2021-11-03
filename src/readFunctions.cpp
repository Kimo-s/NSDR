#include "XOPStandardHeaders.h"			// Include ANSI headers, Mac headers, IgorXOP.h, XOP.h and XOPSupport.h
#include "SXMreader.h"
#include "iostream"
#include "fstream"
#include "string"
#include "stdio.h"
#include "filesystem"
#include "stdlib.h"
#include "math.h"
#include <iostream>
#include <fstream>
#include <filesystem>
namespace fs = std::filesystem;
using namespace std;


extern "C" int
readDATFile(readSXMFileParams * p) {
	char filepath[MAX_OBJ_NAME + 1];
	FILE* fp;
	char buff[10000];

	float startOfBias;
	float endOfBias;

	GetCStringFromHandle(p->fp, filepath, MAX_OBJ_NAME);
	if (fopen_s(&fp, filepath, "r") != 0) {
		return FILE_NOT_FOUND;
	}

	//checks if the file is a dat file
	if (strcmp(fs::path(filepath).extension().string().c_str(), ".dat") != 0) {
		sprintf(buff, "Error: A none DAT file was found (the file path: %s)\n", fs::path(filepath).string().c_str());
		XOPNotice(buff);
		return FILE_NOT_FOUND;
	}

	int found_headers = 0;
	char message[1000];
	while (found_headers != 3) {
		fscanf_s(fp, "%s", buff, sizeof(buff));
		if (strcmp(buff, "[DATA]") == 0) {
			fgets(buff, sizeof(buff), fp);
			fgets(buff, sizeof(buff), fp);
			found_headers++;
		}
		else if (strcmp(buff, "Sweep") == 0 || strcmp(buff, "Spectroscopy>Sweep") == 0) {
			fscanf_s(fp, "%s", buff, sizeof(buff));
			if (strcmp(buff, "Start") == 0) {
				fscanf_s(fp, "%s", buff, sizeof(buff));
				if (strcmp(buff, "(V)") == 0) {
					fscanf_s(fp, "%E", &startOfBias);
				}
				else {
					startOfBias = atof(buff);
				}
				//sprintf(message, "startOfBias = %e\n", startOfBias);
				//XOPNotice(message);
				found_headers++;
			}
			else if (strcmp(buff, "End") == 0) {
				fscanf_s(fp, "%s", buff, sizeof(buff));
				if (strcmp(buff, "(V)") == 0) {
					fscanf_s(fp, "%E", &endOfBias);
				}
				else {
					endOfBias = atof(buff);
				}
				//sprintf(message, "endOfBias = %e\n", endOfBias);
				//XOPNotice(message);
				found_headers++;
			}
		}

	}

	int graphs = 0;
	char* nextToken;
	char* token = strtok_s(buff, " ,\t\n", &nextToken);
	fgets(buff, sizeof(buff), fp);
	while (token != NULL) {
		token = strtok_s(NULL, " ,\t\n", &nextToken);
		graphs++;
	}
	graphs = graphs - 1;


	int points = 1;
	while (fgets(buff, sizeof(buff), fp) != NULL) {
		points++;
	}
	rewind(fp);

	//find labels for each column to name the waves after them
	char* labelLine = (char*)malloc(10000);
	while (1) {
		fscanf_s(fp, "%s", buff, sizeof(buff));
		if (strcmp(buff, "[DATA]") == 0) {
			fgets(buff, sizeof(buff), fp);
			fgets(labelLine, 10000, fp);
			break;
		}
	}
	std::vector <waveHndl> wavePtrs(graphs);
	std::vector <std::string> labels;
	std::string labelLineString(labelLine);

	labelLineString.erase(0, labelLineString.find(")") + 1);
	size_t nextBracketPos;
	while (1) {
		nextBracketPos = labelLineString.find(")");
		if (nextBracketPos == std::string::npos) {
			break;
		}
		labels.push_back(labelLineString.substr(labelLineString.find_first_not_of(" \n\r\t\f\v"), nextBracketPos - 3));
		size_t whitespacepos;
		while (1) {
			whitespacepos = labels.at(labels.size() - 1).find_first_of(" []");
			if (whitespacepos == std::string::npos) {
				break;
			}
			labels.at(labels.size() - 1).replace(whitespacepos, 1, 1, '_');
		}
		labelLineString.erase(0, nextBracketPos + 1);
	}





	int result;
	char waveName[MAX_OBJ_NAME + 1];
	CountInt dimensionSizes[MAX_DIMENSIONS + 1];
	int err;
	double offset = startOfBias;
	double delta = (endOfBias - startOfBias) / (points - 1);
	DataFolderHandle folder;
	DataFolderHandle checker;
	DataFolderHandle parentFolder;

	MemClear(dimensionSizes, sizeof(dimensionSizes));
	dimensionSizes[ROWS] = points;
	GetCStringFromHandle(p->waveName, waveName, MAX_OBJ_NAME);
	if (GetCurrentDataFolder(&parentFolder)) {
		return CANT_FIND_FOLDER;
	}
	/*if (GetNamedDataFolder(parentFolder, waveName, &checker) != 0) {
		return CANT_FIND_FOLDER;
	}*/

	//if (checker == NULL) {
	if (NewDataFolder(parentFolder, waveName, &folder) != 0) {
		return CANT_FIND_FOLDER;
	}
	//}
	/*else {
		return CANT_FIND_FOLDER;
	}*/



	char name[MAX_OBJ_NAME + 1];
	for (int i = 0; i < graphs - 1; i++) {
		waveHndl temp;
		sprintf(name, "%s", labels.at(i).c_str());
		if (result = MDMakeWave(&temp, name, folder, dimensionSizes, NT_FP32, 1)) {
			return result;
		}
		wavePtrs.at(i) = temp; //(float*)WaveData(p->result);
		if (err = MDSetWaveScaling(temp, ROWS, &delta, &offset)) {
			return err;
		}
	}
	p->result = wavePtrs.at(0);



	float datatemp;
	int y = 0;
	while (fgets(buff, sizeof(buff), fp) != NULL) {
		char* nextToken;
		char* token = strtok_s(buff, " ,\t\n", &nextToken);
		for (int x = 0; x < graphs - 1; x++) {
			token = strtok_s(NULL, " ,\t\n", &nextToken);
			sscanf_s(token, "%E", &datatemp, (unsigned)sizeof(float));

			IndexInt cords[1];
			cords[0] = y;
			double value[2];
			value[0] = datatemp;
			MDSetNumericWavePointValue(wavePtrs.at(x), cords, value);
		}
		y++;
	}

	fclose(fp);
	free(labelLine);

	return 0;
}


float ReverseFloat(const float inFloat)
{
	float retVal;
	char* floatToConvert = (char*)&inFloat;
	char* returnFloat = (char*)&retVal;

	// swap the bytes into a temporary buffer
	returnFloat[0] = floatToConvert[3];
	returnFloat[1] = floatToConvert[2];
	returnFloat[2] = floatToConvert[1];
	returnFloat[3] = floatToConvert[0];

	return retVal;
}


extern "C" int
read3ds(readSXMFileParams * p) {
	char filepath[MAX_OBJ_NAME + 1];
	std::ifstream file;
	char buff[1000];

	float startOfBias;
	float endOfBias;

	GetCStringFromHandle(p->fp, filepath, MAX_OBJ_NAME);
	file.open(filepath, ios::binary);


	//checks if the file is a dat file
	if (strcmp(fs::path(filepath).extension().string().c_str(), ".3ds") != 0) {
		sprintf(buff, "Error: A none DAT file was found (the file path: %s)\n", fs::path(filepath).string().c_str());
		XOPNotice(buff);
		return FILE_NOT_FOUND;
	}

	int width;
	int height;
	vector <string> channels;
	vector <string> parameters;
	int points;

	int headersNeeded = 6;
	int headersfound = 0;

	// 0d 0a
	string line;
	if (file.is_open()) {
		while (getline(file, line)) {
			if (headersfound == headersNeeded) {
				break;
			}
			if (line.find(string("Grid dim")) != string::npos) {
				int equalpos = line.find(string("\""));
				int xpos = line.find(string("x"));

				string token = line.substr(equalpos + 1, line.find(string("x")) - (equalpos + 1));
				width = std::stoi(token, nullptr);

				token = line.substr(xpos + 1, line.size() - xpos - 3);
				height = stoi(token, nullptr);
				headersfound++;
			}
			else if (line.find(string("Sweep Signal")) != string::npos) {
				line.erase(0, line.find("\"") + 1);
				channels.push_back(line.substr(0, line.find("\"")));
				headersfound++;
			}
			else if (line.find(string("Fixed parameters")) != string::npos) {
				int equalpos = line.find(string("\""));

				line.erase(0, equalpos + 1);
				parameters.push_back(line.substr(0, line.find(";")));

				line.erase(0, line.find(";") + 1);
				parameters.push_back(line.substr(0, line.find("\"")));
				headersfound++;
			}
			else if (line.find(string("Experiment parameters")) != string::npos) {
				line.erase(0, line.find(string("\"")) + 1);

				while (line.find(";") != string::npos) {
					parameters.push_back(line.substr(0, line.find(";")));
					line.erase(0, line.find(";") + 1);
				}
				parameters.push_back(line.substr(0, line.find("\"")));

				/*for (int i = 0; i < parameters.size(); i++) {
					cout << parameters[i] << '\n';
				}*/

				headersfound++;
			}
			else if (line.find(string("Points")) != string::npos) {
				line.erase(0, line.find("=") + 1);

				points = stoi(line);

				headersfound++;
			}
			else if (line.find(string("Channels")) != string::npos) {
				line.erase(0, line.find(string("\"")) + 1);

				while (line.find(";") != string::npos) {
					channels.push_back(line.substr(0, line.find(";")));
					line.erase(0, line.find(";") + 1);
				}
				channels.push_back(line.substr(0, line.find("\"")));

				// Replace all spaces and square brackets with an underscore
				size_t whitespacepos;
				for (int i = 0; i < channels.size(); i++) {
					channels.at(i) = channels.at(i).substr(0, channels.at(i).find("("));
					while (1) {
						whitespacepos = channels.at(i).find_first_of(" []");
						if (whitespacepos == std::string::npos) {
							break;
						}
						channels.at(i).replace(whitespacepos, 1, 1, '_');
					}
				}
				headersfound++;
			}
		}
	}

	// seek the 
	char c[4];
	file.seekg(0);
	while(1) {
		file.read(c, 1);
		//std::cout << (void*)c[0] << '\n';
		if ((int)c[0] == 0x3a) {
			file.read(c, 1);
			if ((int)c[0] == 0x0d) {
				file.read(c, 1);
				if ((int)c[0] == 0x0a) {
					break;
				}
			}
		}
	}

	char waveName[MAX_OBJ_NAME + 1];
	CountInt dimensionSizes[MAX_DIMENSIONS + 1];
	DataFolderHandle folder;
	DataFolderHandle parentFolder;
	int result;
	int err;

	MemClear(dimensionSizes, sizeof(dimensionSizes));
	dimensionSizes[ROWS] = width;
	dimensionSizes[COLUMNS] = height;
	dimensionSizes[LAYERS] = points;

	GetCStringFromHandle(p->waveName, waveName, MAX_OBJ_NAME);
	if (GetCurrentDataFolder(&parentFolder)) {
		return CANT_FIND_FOLDER;
	}
	if (NewDataFolder(parentFolder, waveName, &folder) != 0) {
		return CANT_FIND_FOLDER;
	}

	float ex;
	vector <float> parametersFirst(parameters.size());
	for (int i = 0; i < parameters.size(); i++) {
		file.read((char*)&ex, 4);
		parametersFirst[i] = ReverseFloat(*(float*)&ex);
	}
	
	vector <float> parametersLast (parameters.size());
	std::vector <std::vector<float>> data;
	for (int i = 0; i < width*height; i++) {
		for (int s = 0; s < channels.size()-1; s++) {
			std::vector <float> tempData (points);
			for (int q = 0; q < points; q++) {
				file.read((char*)&ex, 4);
				tempData[q] = ReverseFloat(*(float*)&ex);
				/*sprintf(buff, "Value at (%d,%d) = %E\n", s, q, tempData[q]);
				XOPNotice(buff);
				DoUpdate();*/
			}
			data.push_back(tempData);
		}
		int next = i + 2;
		if (next == width * height) {
			for (int a = 0; a < parametersLast.size(); a++) {
				file.read((char*)&ex, 4);
				parametersLast[a] = ReverseFloat(*(float*)&ex);
			}
		}
		else {
			for (int a = 0; a < parametersLast.size(); a++) {
				file.read((char*)&ex, 4);
			}
		}
	}


	/*sprintf(buff, "%d    %d    %d\n", data.size(), data[0].size(), points);
	XOPNotice(buff);
	for (int i = 0; i < data.size(); i++) {
		for (int s = 0; s < data[0].size(); s++) {
			sprintf(buff, "Value at (%d,%d) = %E\n", i, s, data[i][s]);
			XOPNotice(buff);
		}
	}*/

	double offset;
	double delta;
	std::vector <waveHndl> waveptrs (channels.size()-1);
	for (size_t i = 0; i < channels.size() - 1; i++) {
		sprintf(waveName, "%s", channels[i + 1].c_str());
		if (result = MDMakeWave(&waveptrs[i], waveName, folder, dimensionSizes, NT_FP32, 1)) {
			return result;
		}
		offset = 0;
		if (height > 1) {
			offset = parametersFirst[2];
			delta = parametersLast[2] - parametersFirst[2] / (width - 1);
		}
		else {
			delta = sqrt(pow(parametersLast[2] - parametersFirst[2], 2) + pow(parametersLast[3] - parametersFirst[3], 2)) / (width - 1);
		}
		if (err = MDSetWaveScaling(waveptrs[i], ROWS, &delta, &offset)) {
			return err;
		}
		if (height > 1) {
			offset = parametersFirst[3];
			delta = parametersLast[3] - parametersFirst[3] / (height - 1);
		}
		else {
			delta = 1;
		}
		if (err = MDSetWaveScaling(waveptrs[i], COLUMNS, &delta, &offset)) {
			return err;
		}
		offset = parametersFirst[1];
		delta = (parametersLast[0] - parametersFirst[1]) / (points - 1);
		if (err = MDSetWaveScaling(waveptrs[i], LAYERS, &delta, &offset)) {
			return err;
		}
		MDSetWaveUnits(waveptrs[i], 0, "m");
		MDSetWaveUnits(waveptrs[i], 1, "m");
		MDSetWaveUnits(waveptrs[i], 2, "V");
	}
	p->result = waveptrs[0];



	IndexInt indices[MAX_DIMENSIONS];
	double value[2];
	size_t waveNum = 0;
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			indices[ROWS] = x;
			indices[COLUMNS] = y;
				for (int w = 0; w < channels.size() - 1; w++) {
					for (int z = 0; z < points; z++) {
						indices[LAYERS] = points-1-z;
						value[0] = data[waveNum][z];
						MDSetNumericWavePointValue(waveptrs[w], indices, value);
					}
					waveNum++;
				}
		}
	}

	file.close();
	return 0;
}
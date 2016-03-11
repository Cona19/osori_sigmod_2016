#include "harness.h"
#include <iostream>
#include <stdlib.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <string>

std::string CONFIG_FILE_PATH = "./config.txt";
std::string DATA_PATH = "./data/";
std::string EXE_PATH = "../bin/spp";
unsigned BATCH_SIZE = 1000;
unsigned long MAX_FAILED_QUERIES = 1;
unsigned START_CASE_NUM = 1;
unsigned END_CASE_NUM = 1;

int parse_config_line(std::string line, std::string *property, std::string *val){
	std::string delimiter = " ";
	size_t pos = line.find(delimiter);
	if(pos == std::string::npos){
		std::cerr << "config file is not correct : " << line << std::endl;
		exit(EXIT_FAILURE);
	}
	*property = line.substr(0, pos);
	*val = line.substr(pos + delimiter.length(), line.length());
	
	return EXIT_SUCCESS;
}

int init_config(std::string config_file_path){
	std::ifstream config_file(config_file_path);
	std::string line;
	std::string property, val;
	if (!config_file){
		std::cerr << "Cannot open config file" << std::endl;
		exit(EXIT_FAILURE);
	}
	while(1){
		if (!std::getline(config_file, line)){
			break;
		}
			
		parse_config_line(line, &property, &val);
		std::transform(property.begin(), property.end(), property.begin(), ::toupper);
		if (property.compare("BATCH_SIZE") == 0){
			BATCH_SIZE = std::stoul(val, NULL, 10);
			std::cout << "Batch_size : " << val << std::endl;
		}
		else if (property.compare("MAX_FAILED_QUERIES") == 0){
			MAX_FAILED_QUERIES = std::stoul(val, NULL, 10);
			std::cout << "Max_failed_queries : " << val << std::endl;
		}
		else if (property.compare("START_CASE_NUM") == 0){
			START_CASE_NUM = std::stoul(val, NULL, 10);
			std::cout << "Start_case_num : " << val << std::endl;
		}
		else if (property.compare("END_CASE_NUM") == 0){
			END_CASE_NUM = std::stoul(val, NULL, 10);
			std::cout << "End_case_num : " << val << std::endl;
		}
		else if (property.compare("DATA_PATH") == 0){
			if (val.back() != '/') val += '/';
			DATA_PATH = val;
			std::cout << "Data_path : " << val << std::endl;
		}
		else if (property.compare("EXE_PATH") == 0){
			EXE_PATH = val;
			std::cout << "EXE_path : " << val << std::endl;
		}
	}
	return EXIT_SUCCESS;
}

int main(int argc, char *argv[]){
	double tot_elapsed_sec = 0;
	double elapsed_sec = 0;
	unsigned long tot_cnt_failure = 0 ;
	unsigned long cnt_failure = 0;
	//use basic usage of harness program
	if (argc == 5){
		if (run_harness(argv[1], argv[2], argv[3], argv[4], &elapsed_sec, &cnt_failure) == EXIT_FAILURE){
			std::cerr << "Failed to test" << std::endl;
		}
		std::cout << elapsed_sec << " sec is elapsed." << std::endl;
		std::cout << cnt_failure << " cases are not correct." << std::endl;
		return EXIT_SUCCESS;
	}
	//handle config file from another path.
	else if (argc == 2){
		CONFIG_FILE_PATH = argv[1];
	}
	//handle config file from default path.
	init_config(CONFIG_FILE_PATH);

	//run harness from start case to end case
	for (unsigned i = START_CASE_NUM; i <= END_CASE_NUM; i++){
		std::string init_file_path = DATA_PATH + "init-file" + std::to_string(i) + ".txt";
		std::string workload_file_path = DATA_PATH + "workload-file" + std::to_string(i) + ".txt";
		std::string result_file_path = DATA_PATH + "result-file" + std::to_string(i) + ".txt";

		if (run_harness(init_file_path.c_str(), workload_file_path.c_str(), result_file_path.c_str(), EXE_PATH.c_str(), &elapsed_sec, &cnt_failure) == EXIT_FAILURE){
			std::cerr << "Failed to test" << std::endl;
		}
		std::cout << "test case " << i << std::endl;
		std::cout << elapsed_sec << " sec is elapsed." << std::endl;
		std::cout << cnt_failure << " cases are not correct." << std::endl;
		tot_elapsed_sec += elapsed_sec;
		tot_cnt_failure += cnt_failure;
	}
	std::cout << "-------statistics------" << std::endl;
	std::cout << "total elapsed sec : " << tot_elapsed_sec << std::endl;
	std::cout << "total number of failure : " << tot_cnt_failure << std::endl;
	return EXIT_SUCCESS;
}

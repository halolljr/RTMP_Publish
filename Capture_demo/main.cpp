#include "PushWork_.h"
#include <iostream>

int main() {
	// 设置控制台输出为 UTF-8,仅仅是控制台的输出是utf-8
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
	PushWork pushWork;
	if (!pushWork.Init()) {
		std::cerr << "Failed to initialize PushWork" << std::endl;
		return -1;
	}

	pushWork.Start();
	std::cout << "PushWork started..." << std::endl << "Press Enter to stop..." << std::endl;
	std::cin.get();

	pushWork.Close();
	std::cout << "PushWork stopped." << std::endl;
	return 0;
}

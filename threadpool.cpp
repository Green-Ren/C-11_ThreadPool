// threadpool.cpp : 定义控制台应用程序的入口点。
//

#include <iostream>
#include <assert.h>
#include <stdexcept>
#include "threadpool.h"

int main()
{
	std::cout << "threadpool test!" << std::endl;

	RGR::threadpool threadpool(10);
	std::future<size_t> ret = threadpool.commit([](size_t a, size_t b) {return a + b; }, 123,456);
	std::cout << "ret value: " << ret.get() << std::endl;
	std::getchar();
	//assert(0);
	throw std::runtime_error("end. ");
    return 0;
}


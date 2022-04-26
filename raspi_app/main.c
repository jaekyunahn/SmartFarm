/* 
 *	Include
 */
#include "main.h"

/*
 *  Main Function
 */
int main(int argc, char ** argv)
{
    /*
     *  Program Entry Point
     */
    printf("Start App : %s\r\n",argv[0]);

    //Start App Entry Point Function
	int res = fAppFunctionEntryPoint(argc,argv);

    //Exit App
	return res;
}
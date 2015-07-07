#include <stdio.h>
#include <stdlib.h>
#include <OpenCL/cl.h>
#include <string.h>
#include <stdbool.h>
//Convert a 2D array index to a 1d index
#define getIndex(i, j, width) (i) * (width) + (j)

static const unsigned int DEFAULT_WORK_GROUP_SIZE = 16;
static const unsigned int DEFAULT_WIDTH = 1024;
static const unsigned int DEFAULT_HEIGHT = 1024;
static const unsigned int DEFAULT_NUM_ITERATIONS = 100;

const char *readFile(const char *filename){
	long int size = 0;
	FILE *file = fopen(filename, "r");
	
	if(!file) {
		fputs("File error.\n", stderr);
		return NULL;
	}
	
	fseek(file, 0, SEEK_END);
	size = ftell(file);
	rewind(file);
	
	char *result = (char *) malloc(size);
	if(!result) {
		fputs("Memory error.\n", stderr);
		return NULL;
	}
	
	if(fread(result, 1, size, file) != size) {
		fputs("Read error.\n", stderr);
		return NULL;
	}
	
	fclose(file);
	return result;
}

static inline void checkError(cl_int errorCode) {
	if(errorCode != CL_SUCCESS) {
		printf("Program failed with error code %i\n", errorCode);
	}
}


void gridInit(float *grid, unsigned int length, unsigned int width) {
	unsigned int j;
	//initialize left row to 100, since the rest is automatically set to 0
	for(j = 0; j < length; j++) {
		grid[getIndex(0, j, width)] = 100;
	}
}

/**
 * Param 1: Grid length/width
 * Param 2: Work group size
 * Param 3: Iterations
 * Param 4: Print result (1/0)
 */
int main(int argc, const char * argv[]) {
	unsigned int width = DEFAULT_WIDTH;
	unsigned int height = DEFAULT_HEIGHT;
	unsigned int workGroupSize = DEFAULT_WORK_GROUP_SIZE;
	unsigned int iterations = DEFAULT_NUM_ITERATIONS;
	bool printResult = false;
	
	if(argc >= 2) {
		width = atoi(argv[1]);
		height = width;
	}
	if(argc >= 3) {
		workGroupSize = atoi(argv[2]);
	}
	if(argc >= 4) {
		iterations = atoi(argv[3]);
	}
	
	if(argc == 5) {
		if(atoi(argv[4]) == 1) {
			printResult = true;
		}
	}
	//atoi upon failure returns 0
	if(width == 0 || height == 0) {
		width = DEFAULT_HEIGHT;
		height = DEFAULT_WIDTH;
	}
	
	if(workGroupSize == 0) {
		workGroupSize = DEFAULT_WORK_GROUP_SIZE;
	}
	
	if(iterations == 0) {
		iterations = DEFAULT_NUM_ITERATIONS;
	}
	
	int gridLength = width * height;
	size_t gridSize = gridLength * sizeof(float);

	//OpenCL only supports float by default
	float *lastGrid = malloc(gridSize);
	float *currentGrid = malloc(gridSize);
	
	gridInit(lastGrid, width, height);
	gridInit(currentGrid, width, height);

	//Initialize the things we will need to work with OpenCL
	cl_context context;
	cl_command_queue commandQueue;
	cl_program program;
	cl_kernel kernel;
	
	size_t dataBytes;
	size_t kernelLength;
	cl_int errorCode;
	
	cl_mem lastGridBuffer;
	cl_mem currentGridBuffer;
	
	cl_device_id* devices;
	cl_device_id gpu;

	context = clCreateContextFromType(0, CL_DEVICE_TYPE_ALL, 0, NULL, &errorCode);
	checkError(errorCode);
	
	errorCode = clGetContextInfo(context, CL_CONTEXT_DEVICES, 0, NULL, &dataBytes);
	devices = malloc(dataBytes);
	errorCode |= clGetContextInfo(context, CL_CONTEXT_DEVICES, dataBytes, devices, NULL);
	
	gpu = devices[0];
	
	commandQueue = clCreateCommandQueue(context, gpu, 0, &errorCode);
	checkError(errorCode);
	
	lastGridBuffer = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, gridSize, lastGrid, &errorCode);
	currentGridBuffer = clCreateBuffer(context, CL_MEM_READ_WRITE, gridSize, NULL, &errorCode);
	checkError(errorCode);
	
	const char* programBuffer = readFile("kernel.cl");
	kernelLength = strlen(programBuffer);
	
	program = clCreateProgramWithSource(context, 1, (const char **)&programBuffer, &kernelLength, &errorCode);
	checkError(errorCode);
	
	errorCode = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
	checkError(errorCode);
	
	kernel = clCreateKernel(program, "diffusion", &errorCode);
	checkError(errorCode);
	
	size_t localWorkSize[2] = {workGroupSize, workGroupSize}, globalWorkSize[2] = {width, height};
	
	for(unsigned int count = 0; count < iterations; count++) {

		errorCode = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&lastGridBuffer);
		errorCode |= clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *)&currentGridBuffer);
		errorCode |= clSetKernelArg(kernel, 2, sizeof(cl_mem), (void *)&width);
		checkError(errorCode);

		errorCode = clEnqueueNDRangeKernel(commandQueue, kernel, 2, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL);
		checkError(errorCode);
		
		//Current buffer -> Last buffer for the next iteration
		clEnqueueCopyBuffer(commandQueue, currentGridBuffer, lastGridBuffer, 0, 0, gridSize, 0, NULL, NULL);
	}
	
	errorCode = clEnqueueReadBuffer(commandQueue, currentGridBuffer, CL_TRUE, 0, gridSize, currentGrid, 0, NULL, NULL);
	checkError(errorCode);
	if(printResult) {
		for(int i = 0; i < width; i++) {
			for(int j = 0; j < height; j++) {
				printf("%8.3f,", currentGrid[getIndex(j, i, width)]);
			}
			printf("\n");
		}
		printf("\n");
	}
    return 0;
}

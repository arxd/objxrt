
#define GLFW_INCLUDE_ES2
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <malloc.h>

#define ABORT(code, fmt, ...)   _abort(code, __FILE__, __LINE__, fmt, ## __VA_ARGS__)
void _abort(int code, char *file, int line, const char *fmt, ...)
{
	if (errno)
		perror("Errno:");
	fprintf(stderr, "%s:%d:", file, line);
	va_list args;
	va_start (args, fmt);
	vfprintf (stderr, fmt, args);	
	va_end (args);
	fprintf(stderr, "\n");
	fflush(stderr);
	exit(code);
}

int compile_shader(const char **src, int nlines, GLenum type, int row_off)
{
	GLuint shader;
	// Compile Vertex Shader
	shader = glCreateShader(type);
	if (!shader)
		ABORT(9, "Couldn't create shader");
	glShaderSource(shader, nlines, src, NULL);
	glCompileShader(shader);	
	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (!status) {
		GLint len = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
		printf("FAIL\n");
		if ( len > 0 ) {
			char* log = alloca(sizeof(char) * len);
			glGetShaderInfoLog(shader, len, NULL, log);
			char *lines[1024];
			int nlines = 0;
			int i=0;
			char *prev = log;
			while(log[i]) {
				if (log[i] == '\n') {
					log[i] = 0;
					lines[nlines] = strdup(prev);
					++nlines;
					prev = &log[i+1];
				}
				++i;
			}
			int prev_row = -1;
			int prev_col = 0;
			for (i=0; i < nlines; ++i) {
				int j=0;
				while(lines[i][j++] != ':');
				while(lines[i][j++] != ':');
				lines[i][j] = 0;
				int id,row,col;
				sscanf(lines[i], "%d:%d(%d)", &id, &row, &col);
				if (prev_row >=0 && prev_row != row) {
					printf("%s", src[prev_row-1]);
					for (int q=0; q < prev_col-1; ++q)
						printf("%c", (src[prev_row-1][q] == '\t')?'\t':' ');
					printf("^\n");
				}
				prev_row = row;
				prev_col = col;
				printf("%d:%d %s\n", row+row_off, col, &lines[i][j+1]);
			}
			printf("%s", src[prev_row-1]);
			for (int q=0; q < prev_col-1; ++q)
				printf("%c", (src[prev_row-1][q] == '\t')?'\t':' ');
			printf("^\n");
		}
		return 0;
	}
	printf("Done\n");
	return 1;
}


int main(int argc, char*argv[])
{
	char * line = NULL;
	size_t len = 0;
	ssize_t read;
	
	if (argc < 3)
		ABORT(1, "Pass an input and output file");
	
	FILE *fp = fopen(argv[1], "r");
	if (!fp)
		ABORT(1, "Couldn't open %s for reading", argv[1]);
	FILE *fout = fopen(argv[2], "w");
	if (!fout) {
		fclose(fp);
		ABORT(2, "Couldn't open %s for writing", argv[2]);
	}
	
	if (!glfwInit())
		ABORT(1, "GLFW Init failed");
	
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	
	//~ const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
	//~ printf("MODE: %d,%d  %f %d\n", mode->width, mode->height, (double)mode->width/(double)mode->height, mode->refreshRate);
	GLFWwindow *window = glfwCreateWindow(256, 144, "Glyphy Graphics", NULL, NULL);
	glfwMakeContextCurrent(window);
	
	printf("\t%s\n\tprecision: ", glGetString(GL_SHADING_LANGUAGE_VERSION));
	int range[2], precision;
	glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_LOW_FLOAT, range, &precision);
	printf("%d, ",precision);	
	glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_MEDIUM_FLOAT, range, &precision);
	printf("%d, ",precision); 
	glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_HIGH_FLOAT, range, &precision);
	printf("%d\n",precision); 
	
	int line_number = 0;
	char *src[4096] = {0};
	int src_lines=0;
	int line_offset;
	char *name = 0;
	while (++line_number, (read = getline(&line, &len, fp)) != -1) {
		if (read > 4 && line[0] == '/' && line[1] =='/' && line[2] == '/' && line[3]=='/') {
			if (name) {
				printf("\tCompile <%s> ", name);
				if (compile_shader((const char**)src, src_lines, (name[0]=='F')?GL_FRAGMENT_SHADER:GL_VERTEX_SHADER, line_offset)) {
					fprintf(fout, "#define %s \"\\n\\\n", name);
					for (int i=0; i < src_lines; ++i) {
						src[i][strlen(src[i])-1] = 0;
						fprintf(fout, "%s\\n\\\n", src[i]);
					}
					fprintf(fout, "\"\n\n");
				} else {
					exit(-1);
				}
				
			}
			
			// Start a new program
			line[strlen(line)-1]= 0;
			name = strdup(&line[4]);
			src[0] = 0;
			src_lines = 0;
			line_offset = line_number;
		} else {
			//line[strlen(line)-1] = 0;
			src[src_lines] = strdup(line);
			++src_lines;
		}
	}

	fclose(fp);
	fclose(fout);
	if (line)
		free(line);
	
	glfwDestroyWindow(window);
	glfwTerminate();

	
	
}

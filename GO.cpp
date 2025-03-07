
//System includes
#include <ctime>

//Aplication includes
#include "rnd.h"
#include "centerpoint.h"
#include "utils.h"
//#include "GO.h"
//#include <Windows.h>

//OpenGL includes
#include <glew-1.10.0\include\GL\glew.h>
#include <glfw-2.7.6\include\GL\glfw.h> //Toolkit
#include <glm-0.9.4.0\glm\glm.hpp> //Matrix manipulations
#include <glm-0.9.4.0\glm\gtc\matrix_transform.hpp> //View transforms
//#include <glm-0.9.4.0\glm\gtc\quaternion.hpp> //Quaternions operations

// Macro specific keys from keyboard
#define GLFW_KEY_A   65
#define GLFW_KEY_D   68
#define GLFW_KEY_S   83
#define GLFW_KEY_W   87

//Pi
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Cute little macro that computes the minimum of two numbers
#define min(a,b) (((a)<(b))?(a):(b))

// Window dimensions and A/R
static int	CurrentWidth = 800,
			CurrentHeight = 600,
			WindowHandle = 0;
static float aspectRatio = (float)CurrentWidth/CurrentHeight;

// View globals
static float horizontalAngle = 0.0f, // Initial horizontal angle
			 verticalAngle =   0.0f, // Initial vertical angle
			 //FoV = 75.0f, // Initial Field of View //FOV is not used as of 04Dec2013
			 Scale = 0.4f, //Scale
			 panx, pany; //Pan values

static const float keySpeed = 0.1f, 
				   mouseCursorSpeed = 0.2f,
				   mouseWheelSpeed = 0.05f;

//static glm::vec3 vPosition = glm::vec3( 0.0f, 5.0f, 2.0f ), // Initial camera position
//				 vDirection, 
//				 vRight, 
//				 vUp;

static glm::mat4 ViewMatrix, 
				 ProjectionMatrix, 
				 MVP;

static const glm::mat4 ModelMatrix = glm::mat4(1.0f);

static int do_redraw = 1, // Do redraw? (repaint the window only as needed)
		   xpos = 0, // Mouse position
		   ypos = 0, // Mouse position
		   Points2D =	1,	//Display the 2D points?
		   Points3D =	1,	//Display the 3D points?
		   Separator =	1;	//Display the Separator?
		   //Centerpoint = 1; //Display the Centerpoint?

// IDs for VBOs and shaders
GLuint ProgramID, //ID of our shader
	   MatrixID, //ID for view matrix transformations3
	   VertexShaderId,
	   FragmentShaderId,
	   VertexArrayID,
	   VertexBufferID,
	   ColorBufferID;

// GLSL vertex shader
static const GLchar* VertexShader =
{
    "#version 330 core\n"\
 
    "layout(location = 0) in vec3 vertexPosition_modelspace;\n"\
    "layout(location = 1) in vec3 vertexColor;\n"\
    "out vec3 fragmentColor;\n"\

	"uniform mat4 MVP;\n"\
 
    "void main(void)\n"\
    "{\n"\
    "   gl_Position =  MVP * vec4(vertexPosition_modelspace,1);\n"\
    "   fragmentColor = vertexColor;\n"\
    "}\n"
};

// GLSL fragment shader
static const GLchar* FragmentShader =
{
    "#version 330 core\n"\
 
    "in vec3 fragmentColor;\n"\
    "out vec3 color;\n"\
 
    "void main(void)\n"\
    "{\n"\
    "   color = fragmentColor;\n"\
    "}\n"
};


// Recompute the MVP for current view setup
void UpdateMVP(void)
{
	//printf("Vangle %f\n", verticalAngle);
	//printf("Hangle %f\n", horizontalAngle);
	//printf("Position [%f;%f;%f]\n",vPosition.x,vPosition.y,vPosition.z);

	//// First calculate view Direction : Spherical coordinates to Cartesian coordinates conversion ( this is only useful if we use Perspective = glm::lookAt(..) )
	//vDirection.x = cos(verticalAngle) * sin(horizontalAngle);
	//vDirection.y = sin(verticalAngle);
	//vDirection.z = cos(verticalAngle) * cos(horizontalAngle);

	//// Now calculate the Right vector (remember the World has that weird orientation away from screen)
	//vRight.x = sin(horizontalAngle - M_PI/2.0f);
	//vRight.y = 0.0f;
	//vRight.z = cos(horizontalAngle - M_PI/2.0f);

	//// Finally, the Up vector
	//vUp = glm::cross( vDirection, vRight );

	//// Now calculate the MVP matrices one at a time
	//ProjectionMatrix = glm::perspective(FoV, //Field of view
	//									aspectRatio, //Aspect ratio
	//									0.1f, //Screen units
	//									100.0f); //World units      //This Projection Matrix will always create a perspective view

	//// Camera matrix
	//ViewMatrix = glm::lookAt(vPosition,           // Camera is here
	//						 vPosition+vDirection, // and looks here : at the same position, plus "direction"
	//						 vUp);                // Head is up (set to 0,-1,0 to look upside-down)

	GLfloat SPHERE_RADIUS = 50.0f;
	GLint windH, windW;
	glfwGetWindowSize(&windW, &windH); //Get current window dimensions
	GLfloat xextent = (GLfloat) windW / min(windH,windW); //Calculate the bounding box
	GLfloat yextent = (GLfloat) windH / min(windH,windW);

	// Projection matrix
	ProjectionMatrix = glm::ortho(-xextent * SPHERE_RADIUS * Scale, xextent * SPHERE_RADIUS * Scale,
								  -yextent * SPHERE_RADIUS * Scale, yextent * SPHERE_RADIUS * Scale,
								   5 * SPHERE_RADIUS, -5 * SPHERE_RADIUS); //This will always create an isometric view

	// View Matrix
	glm::mat4 ViewTranslate = glm::translate( glm::mat4(1.0f), glm::vec3(panx, pany, 0.0f) );
	glm::mat4 ViewRotateX   = glm::rotate( ViewTranslate, horizontalAngle, glm::vec3(0.0f, 1.0f, 0.0f) );
	glm::mat4 ViewMatrix    = glm::rotate( ViewRotateX, verticalAngle, glm::vec3(1.0f, 0.0f, 0.0f) ); // Compute it as Translation plus two Rotations
					   
	// Model matrix is always Identity Matrix for me (no scalling, shearing, translation, rotation or what-not)
	MVP = ProjectionMatrix * ViewMatrix * ModelMatrix; // Remember, matrix multiplication is the other way around
	glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]); // Send our transformation to the currently bound shader, in the "MVP" uniform
}

// Mouse position callback function
void GLFWCALL MousePosFun(int x, int y)
{
	// Reorient only on left mouse button pressed
	if (glfwGetMouseButton(GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
	{
		// Calculate new orientation
		horizontalAngle += mouseCursorSpeed * float(x - xpos); //Some people call this "angleX"
		verticalAngle   += mouseCursorSpeed * float(y - ypos); // ----||---- "angleY"
		do_redraw = 1;
	}

	// Pan if the middle button was pressed
	if (glfwGetMouseButton(GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS)
	{
		panx += mouseCursorSpeed * float(x - xpos); //Pan value X
		pany -= mouseCursorSpeed * float(y - ypos); //Pan value Y (X,Y are screen coordinates not model or world)
		//vPosition += vRight * panx * 10.0f; // Strafe left/right ( this is only useful if we use Perspective = glm::lookAt(..) )
		//vPosition += vUp * pany * 10.0f; // Pan up/down
		do_redraw = 1;
	}

    // Remember cursor position
    xpos = x;
    ypos = y;
}

// Mouse wheel is rotated
void GLFWCALL MouseWheelFun(int wheel)
{
	//float currFoV = FoV + mouseWheelSpeed * wheel; //FOV is not used as of 4Dec2013
	//FoV = (currFoV < 180.0f && currFoV > 0.0f) ? currFoV : FoV; // Field of view can only be between [0 - 180]deg
	GLfloat currScale = Scale + mouseWheelSpeed * wheel;
	Scale = (currScale < 1.0f && currScale > 0.0001f) ? currScale : Scale; //Scale is between 1x smaller to 10000x larger
	glfwSetMouseWheel(0); //Reset the wheel relative position
	do_redraw = 1;
}

// Keyboard input callback function
void GLFWCALL KeyboardFun(int key, int state)
{
	// These are for now panning events
	if (state == GLFW_PRESS){
		switch (key){
		case GLFW_KEY_HOME: //Orthographic projection
			verticalAngle = 0.0f;//-M_PI;
			horizontalAngle = 0.0f;//2.0f * M_PI;
			//vPosition = glm::vec3( 0.0f, 0.0f, 10.0f );
			break;
		case GLFW_KEY_LEFT:
			Points2D = (Points2D) ? 0 : 1; //Toggle the display of 2D points
			break;
		case GLFW_KEY_DOWN:
			Points3D = (Points3D) ? 0 : 1; //Toggle the display of 3D points
			break;
		case GLFW_KEY_RIGHT:
			Separator = (Separator) ? 0 : 1; //Toggle the display of the separator
			break;
		case GLFW_KEY_UP:
			//Centerpoint = (Centerpoint) ? 0 : 1; //Toggle the display of the centerpoint
			break;
		default:
			break; //No default key mapped
		}
		do_redraw = 1;
	}
}

// Recalculate the aspect ratio and limits when window has been resized
void GLFWCALL WindowResize( int w, int h )
{
	CurrentWidth = w > 30 ? w : 30; //To prevent division by 0
	CurrentHeight = h > 30 ? h : 30;

	glViewport(0, 0, w, h);
	glMatrixMode(GL_PROJECTION); /* switch matrix mode */
	glLoadIdentity();
	if (w <= h){
		aspectRatio = (GLfloat)h/(GLfloat)w; //Recalculate the A/R
		glm::ortho(-2.0 * aspectRatio, 2.0 * aspectRatio, -2.0, 2.0);
	}
	else {
		aspectRatio = (GLfloat)w/(GLfloat)h; //Recalculate the A/R
		glm::ortho(-2.0 * aspectRatio, 2.0 * aspectRatio, -2.0, 2.0);
	}
	glMatrixMode(GL_MODELVIEW); /* return to modelview mode */
	
	do_redraw = 1; //Ask repainting
}

// Window has been refreshed, redraw everything
void GLFWCALL windowRefreshFun(void)
{
	do_redraw = 1;
}


// Draw a 2D grid (used to orient better) needs much work
static void drawGrid(float scale, int steps, GLfloat *gridVertex, GLfloat *gridColour)
{
    int i;
    float x, y;

    // Set grid color
    glColor3f(0.0f, 0.5f, 0.5f);
	
    // Horizontal lines
    x =  scale * 0.5f * (float) (steps - 1);
    y = -scale * 0.5f * (float) (steps - 1);
    for (i = 0;  i < steps;  i++)
    {
		gridVertex[4*i+0] = -x;
		gridVertex[4*i+1] = y;
		gridVertex[4*i+2] = x;
		gridVertex[4*i+3] = y;
        //glVertex3f(-x, y, 0.0f);
        //glVertex3f(x, y, 0.0f);
        y += scale;
    }

    // Vertical lines
    x = -scale * 0.5f * (float) (steps - 1);
    y = scale * 0.5f * (float) (steps - 1);
    for (i = 0;  i < steps;  i++)
    {
        glVertex3f(x, -y, 0.0f);
        glVertex3f(x, y, 0.0f);
        x += scale;
    }

	// Draw the Floor
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glDisableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);
	//glVertexPointer(3, GL_FLOAT, 0, zFloorVertices);
	glDrawArrays(GL_LINES, 0, 42);
	//glVertexPointer(3, GL_FLOAT, 0, xFloorVertices);
	glDrawArrays(GL_LINES, 0, 42);
}

void InitWindow(void)
{
	// Initialise GLFW
	if( !glfwInit() )
	{
		fprintf( stderr, "Failed to initialize GLFW\n" );
		exit(EXIT_FAILURE);
	}
		
	glfwOpenWindowHint(GLFW_FSAA_SAMPLES, 4);
	glfwOpenWindowHint(GLFW_OPENGL_VERSION_MAJOR, 3);
	glfwOpenWindowHint(GLFW_OPENGL_VERSION_MINOR, 3);
	glfwOpenWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwSwapInterval(1);

	// Open a window and create its OpenGL context (use mostly default parameters)
	WindowHandle = glfwOpenWindow( CurrentWidth, CurrentHeight, 0,0,0,0,0,0, GLFW_WINDOW );

	if( WindowHandle < 1 )
	{
		fprintf( stderr, "Failed to open GLFW window.\n" );
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	glfwSetWindowTitle( "Geometric Separators" );
}

void Initialize(void)
{
	// Start the show
	InitWindow();

	// Initialize GLEW
	glewExperimental = true; // Needed for core profile
	GLenum GlewInitResult = glewInit(); //Initialize GLEW
	
	if (GLEW_OK != GlewInitResult) {
		fprintf(stderr, "GLEW_ERROR: %s\n", glewGetErrorString(GlewInitResult));
		exit(EXIT_FAILURE);
	}

	int maj, min, rev; //OpenGL version
	glfwGetGLVersion(&maj, &min, &rev);
	fprintf(stdout,"INFO: Using OpenGL %d.%d.%d\n",maj,min,rev);
	fprintf(stdout,"INFO: Using GLEW %s\n",glewGetString(GLEW_VERSION)); //GLEW version

	// Ensure we can capture a continuous panning event (hold down a pan key)
	glfwEnable( GLFW_KEY_REPEAT );
	glClearColor(0.0f, 0.0f, 0.15f, 0.0f); // Dark blue background
	glEnable(GL_DEPTH_TEST); // Enable depth test
	glDepthFunc(GL_LESS); // Accept fragment if it closer to the camera than the former one
	glEnable(GL_CULL_FACE); // Cull triangles which normal is not towards the camera

	// Register callback functions
	glfwSetWindowSizeCallback( WindowResize ); //Window resize
	glfwSetWindowRefreshCallback( windowRefreshFun ); //Window refresh
	glfwSetMousePosCallback( MousePosFun ); //Mouse moves
	glfwSetKeyCallback( KeyboardFun ); //Some key is pressed
	glfwSetMouseWheelCallback( MouseWheelFun ); //Mouse wheel is rotated

	// Generate the VAO(s) (needs to be in the same place with the glewInit() for some magical reason)
	glGenVertexArrays(1, &VertexArrayID);
	glBindVertexArray(VertexArrayID);
}

// Error check for all GL operations
void CheckGLErrors()
{
  GLenum err ;
  while ( (err = glGetError()) != GL_NO_ERROR )
    fprintf( stderr, "OpenGL Error: %s\n", glewGetErrorString ( err ) );
}

// Check if a shader is propperly compiled and attached. This is gold if you have typo's in your shader code
void CheckCompileStatus(GLenum ShaderType, GLuint shader)
{
    // Check shader status
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

    // Detect shader type
    const char* strShaderType = NULL;
    switch(ShaderType){
		case GL_VERTEX_SHADER: strShaderType = "Vertex"; break;
		case GL_FRAGMENT_SHADER: strShaderType = "Fragment"; break;
		case GL_GEOMETRY_SHADER: strShaderType = "Geometry"; break;
		default: break; // Unknown shader type (it shouldn't get here though)
    }

    if (status == GL_FALSE)
    {
        GLint infoLogLength;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLength);
        GLchar* strInfoLog = new GLchar[infoLogLength+1];
        glGetShaderInfoLog(shader,infoLogLength,NULL,strInfoLog);
        fprintf(stderr, "ERROR: %s shader compile error:\n%s\n",strShaderType,strInfoLog);
        delete [] strInfoLog; strInfoLog = NULL;
    }
    else
        fprintf(stdout, "INFO: %s shader compile: OK\n",strShaderType);
}

// Check if the program linked alright (for shaders, not the whole code :D ). This is gold if you have typo's in your shader code
void CheckProgram(GLuint prog)
{
	// Check status
    GLint status, info;
    glGetProgramiv(prog, GL_LINK_STATUS, &status);

	if (status == GL_FALSE)
    {
        GLint infoLogLength;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &infoLogLength);
        GLchar* strInfoLog = new GLchar[infoLogLength+1];
        glGetProgramInfoLog(prog, infoLogLength, NULL, strInfoLog);
        fprintf(stderr, "ERROR: Program link error:\n%s\n",strInfoLog);
        delete [] strInfoLog; strInfoLog = NULL;
    }
    else
	{
		glGetProgramiv(prog, GL_ATTACHED_SHADERS, &status);
		glGetProgramiv(prog, GL_ACTIVE_UNIFORMS, &info);
        fprintf(stdout, "INFO: Program linking: OK. \n\t%d attached shader(s)\n\t%d active uniform(s)\n", status, info);
	}
}

// Creates a vertex array to load onto the display buffer
GLboolean sPoint2VertexArray(sPoint const* sPtIn, unsigned n, GLfloat **gfOut)
{
	*gfOut = new GLfloat[3*n]; //Allocate space here, delete after passing to buffer
	for (unsigned i=0; i<n; i++)
	{
		(*gfOut)[0+3*i] = (GLfloat) sPtIn[i].x;
		(*gfOut)[1+3*i] = (GLfloat) sPtIn[i].y;
		(*gfOut)[2+3*i] = (GLfloat) sPtIn[i].w;
	}
	return GL_TRUE;
}

// Projects a set of 3D points onto a plane (now is the XY plane) (operates with vertex arrays)
GLboolean VA3DtoVA2D(GLfloat const* gfInput, unsigned n, GLfloat **gfOutput)
{
	*gfOutput = new GLfloat[3*n];
	for (unsigned i=0; i<3*n; i++)
		(*gfOutput)[i] = ((i+1) % 3 == 0) ? 0.0f : gfInput[i];
	// Every thirt number (the Z-coordinate) is set to zero, otherwise it's copied from input set
	return GL_TRUE;
}

// Creates a colour array to load onto the display buffer
GLboolean rndColourArray(unsigned n, GLfloat **gfOut)
{
	GLfloat RED   = (rand() % 100 + 1)/100.0f;
	GLfloat GREEN = (rand() % 100 + 1)/100.0f;
	GLfloat BLUE  = (rand() % 100 + 1)/100.0f;

	*gfOut = new GLfloat[3*n]; //Allocate space and load with some colour
	for (unsigned i=0; i<n; i++)
	{
		(*gfOut)[0+3*i] = RED;
		(*gfOut)[1+3*i] = GREEN;
		(*gfOut)[2+3*i] = BLUE;
	}
	return GL_TRUE;
	// Pink: 1.0f 0.1f 0.4f
}

// This is a very peculiar function. It draws a circle but not using sin/cos, using Newton's law! :-)
void ComputeCircle(sPoint const* center, GLfloat r, int iNoSegments, GLfloat **gfVertices) 
{ 
	float theta = 2.0f * M_PI / (iNoSegments-1); 
	float tangetial_factor = tanf(theta);//calculate the tangential factor 
	float radial_factor = cosf(theta);//calculate the radial factor 
	float x = r; //we start at angle = 0 
	float y = 0; 

	*gfVertices = new GLfloat[iNoSegments*3];
    
	// Calculate up until the next-to-last point
	for(int ii = 0; ii < iNoSegments - 1; ii++) 
	{ 
		(*gfVertices)[ii*3+0] = (GLfloat) x + center->x;
		(*gfVertices)[ii*3+1] = (GLfloat) y + center->y;
		(*gfVertices)[ii*3+2] = 0.0f;
        
		//Calculate the tangential vector {radial vector is (x,y)}. Simply flip coordinates and negate one
		float tx = -y; 
		float ty = x; 
        
		//Add the tangential vector 
		x += tx * tangetial_factor; 
		y += ty * tangetial_factor; 
        
		//Correction using the radial factor 
		x *= radial_factor; 
		y *= radial_factor; 
	} 

	//Last point is identical to the first (to have a closed loop)
	for(int ii = 0; ii < 3; ii++) 
		(*gfVertices)[(iNoSegments-1)*3+ii] = (*gfVertices)[ii];

}

// Creates and bounds a VBO to an array of vertices plus colour
void CreateVBO(GLuint *uiN, GLuint const* uiPartSegms)
{
	// Generate the set of points
	sPoint *points = NULL;
	sPoint pntCenter, sPartCenter;
	double dRadius;
	do {
		generatePoints(&points, *uiN);
		getPartition(points, *uiN, &pntCenter, &dRadius, &sPartCenter); // Calculate the partition
	} while (abs(pntCenter.w) < 0.0001f);

	// Arrange the points into an array (vertex position)
	GLfloat *gfVertices1 = NULL; //3D points (the paraboloid)
	sPoint2VertexArray(points, *uiN, &gfVertices1);

	GLfloat *gfVertices2 = NULL; // 2D points (the input set)
	VA3DtoVA2D(gfVertices1, *uiN, &gfVertices2);

	GLfloat *gfVertices3 = NULL; // Partition
	ComputeCircle(&sPartCenter, dRadius, *uiPartSegms, &gfVertices3);

	//GLfloat *gfVertices4 = NULL; //The centerpoint
	//sPoint2VertexArray(&pntCenter, 1, &gfVertices4);

	// Assign vertices colour
	GLfloat *gfColours1 = NULL, *gfColours2 = NULL, *gfColours3 = NULL, *gfColours4 = NULL;
	rndColourArray(*uiN, &gfColours1);
	//sPoint2VertexArray(points, *uiN, &gfColours2); //This makes a nice coloured point set and it seems to never crash (although is incorrect to call this function for colours)
	rndColourArray(*uiN, &gfColours2);
	rndColourArray(*uiPartSegms, &gfColours3);
	//rndColourArray(1, &gfColours4);

	auto auPointsBufferSize = 3 * (*uiN) * sizeof(GLfloat); //How much memory the points take in the buffer (one set, either the 3D or the 2D, they are the same size)
	auto auPartBufferSize   = 3 * (*uiPartSegms) * sizeof(GLfloat); //Partition takes this much space
	//auto auCPBufferSize		= 3 * sizeof(GLfloat); // This is how much the centerpoint occupies in the main buffer
	auto auTotalBufferSize  = 2 * auPointsBufferSize + auPartBufferSize;// + auCPBufferSize; //Calculate how much memory the buffer requires

	// Add the vertex location to the buffer
	glGenBuffers(1, &VertexBufferID);
	glBindBuffer(GL_ARRAY_BUFFER, VertexBufferID);
	glBufferData(GL_ARRAY_BUFFER, auTotalBufferSize, &gfVertices1[0], GL_STATIC_DRAW); //Transfer to GPU memory the 3D points
	glBufferSubData(GL_ARRAY_BUFFER, auPointsBufferSize, auPointsBufferSize, &gfVertices2[0]); //Add the 2D points
	glBufferSubData(GL_ARRAY_BUFFER, 2 * auPointsBufferSize, auPartBufferSize, &gfVertices3[0]); //Add the partition
	//glBufferSubData(GL_ARRAY_BUFFER, 2 * auPointsBufferSize + auPartBufferSize, auCPBufferSize, &gfVertices4[0]); //Add the centerpoint
	
	// Add the colour information to the buffer
	glGenBuffers(1,&ColorBufferID);
	glBindBuffer(GL_ARRAY_BUFFER,ColorBufferID);
	glBufferData(GL_ARRAY_BUFFER, auTotalBufferSize, &gfColours1[0], GL_STATIC_DRAW); //Transfer to GPU memory (this colours both the point sets)
	glBufferSubData(GL_ARRAY_BUFFER, auPointsBufferSize, auPointsBufferSize, &gfColours2[0]); //Add the partition points to the display buffer
	glBufferSubData(GL_ARRAY_BUFFER, 2 * auPointsBufferSize, auPartBufferSize, &gfColours3[0]); //Add the partition points to the display buffer
	//glBufferSubData(GL_ARRAY_BUFFER, 2 * auPointsBufferSize + auPartBufferSize, auCPBufferSize, &gfColours4[0]); //Add the partition points to the display buffer

	//Don't forget to deallocate if you don't need them anymore
	delete [] points; points = NULL;
	delete [] gfVertices1; gfVertices1 = NULL;
	delete [] gfVertices2; gfVertices2 = NULL;
	delete [] gfVertices3; gfVertices3 = NULL;
	delete [] gfColours1;  gfColours1  = NULL;
	delete [] gfColours2;  gfColours2  = NULL;
	delete [] gfColours3;  gfColours3  = NULL;
	//delete [] gfColours4;  gfColours4  = NULL;
 }

// Does what it sais
void DestroyVBO(void)
{
	glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
         
    glBindBuffer(GL_ARRAY_BUFFER, 0);
 
    glDeleteBuffers(1, &ColorBufferID);
    glDeleteBuffers(1, &VertexBufferID);
 
    glBindVertexArray(0);
    glDeleteVertexArrays(1, &VertexArrayID);
}

// Does what it sais
void CreateShaders(void)
{
    // Create and compile the GLSL vertex shader 
    VertexShaderId = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(VertexShaderId, 1, &VertexShader, NULL);
    glCompileShader(VertexShaderId);

	// Check vertex shader if OK
	CheckCompileStatus(GL_VERTEX_SHADER, VertexShaderId);

	// Create and compile the GLSL fragment shader 
    FragmentShaderId = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(FragmentShaderId, 1, &FragmentShader, NULL);
    glCompileShader(FragmentShaderId);

	// Check if fragment shader is OK
	CheckCompileStatus(GL_FRAGMENT_SHADER, FragmentShaderId);

	// Create the program and attach the shaders to it
    ProgramID = glCreateProgram();
    glAttachShader(ProgramID, VertexShaderId);
    glAttachShader(ProgramID, FragmentShaderId);
    glLinkProgram(ProgramID);
    glUseProgram(ProgramID);

	//Check if program linked ok
	CheckProgram(ProgramID);

	// Get a handle for our "MVP" uniform
	MatrixID = glGetUniformLocation(ProgramID, "MVP");
 }

// Does what it sais
void DestroyShaders(void)
{ 
    glUseProgram(0); //Invoke current program
 
    glDetachShader(ProgramID, VertexShaderId);
    glDetachShader(ProgramID, FragmentShaderId);
 
    glDeleteShader(FragmentShaderId);
    glDeleteShader(VertexShaderId);
 
    glDeleteProgram(ProgramID);
}

// Does what it sais
void Cleanup(void)
{
	DestroyVBO();
    DestroyShaders();
}

// Main graphics loop
void runRenderLoop()
{
	// Start by butting the cursor in the middle of the window 
	glfwSetMousePos(CurrentWidth/2, CurrentHeight/2); 

	// Create the VBOs and shaders
	GLuint uiNPoints, uiNSegms = 100;
	CreateVBO(&uiNPoints, &uiNSegms);
	CreateShaders();
	
	do{
		if (do_redraw) //Only redraw if someone posted to do so
		{
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear the screen
			glUseProgram(ProgramID); // Use our shader

			// Recalculate scene (using MVP matrix)
			UpdateMVP();

			// 1rst attribute buffer : vertices
			glEnableVertexAttribArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, VertexBufferID);
			glVertexAttribPointer(
				0,                  // attribute. No particular reason for 0, but must match the layout in the shader.
				3,                  // size (3Dimensional points)
				GL_FLOAT,           // type
				GL_FALSE,           // normalized?
				0,                  // stride
				(void*)0            // array buffer offset
			);

			// 2nd attribute buffer : colors
			glEnableVertexAttribArray(1);
			glBindBuffer(GL_ARRAY_BUFFER, ColorBufferID);
			glVertexAttribPointer(
				1,                                // attribute. No particular reason for 1, but must match the layout in the shader.
				3,                                // size (3 = RGB)
				GL_FLOAT,                         // type
				GL_FALSE,                         // normalized?
				0,                                // stride
				(void*)0                          // array buffer offset
			);

			//// Draw the whole thing (if visibility is enabled for each item)
			if (Points3D)	 glDrawArrays(GL_POINTS, 0, uiNPoints); //Draw points on the paraboloid
			if (Points2D) 	 glDrawArrays(GL_POINTS, uiNPoints, uiNPoints); //Draw points in the plane
			if (Separator)	 glDrawArrays(GL_LINE_STRIP, 2*uiNPoints, uiNSegms); //Draw the partition
			//if (Centerpoint) glDrawArrays(GL_POINTS, 2*uiNPoints + uiNSegms, 1); //One lonely point (the centerpoint)

			// Detach vertex attributes
			glDisableVertexAttribArray(0);
			glDisableVertexAttribArray(1);

			//Stop redrawing stuff
			do_redraw = 0; 

			// Swap buffers
			glfwSwapBuffers();
		}
		
		// Don't loop aimlessly, you'll get dizzy. Wait for new events
        glfwWaitEvents();
	} 
	while( glfwGetKey( GLFW_KEY_ESC ) != GLFW_PRESS &&
		   glfwGetWindowParam( GLFW_OPENED ) ); // Check if the ESC key was pressed or the window was closed

	// Destroy VBOs and Shaders
	Cleanup();

	// Close OpenGL window and terminate GLFW
	glfwTerminate();
	glfwCloseWindow();

	// Hooray! If it got here nothing bad happened
	exit( EXIT_SUCCESS );
}


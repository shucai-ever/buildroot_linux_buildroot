/*
 * This proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * \file EGLPreserve.cpp
 * \brief A sample to show how to use glScissor() and EGL_SWAP_BEHAVIOUR
 *
 * The sample changes between three states:
 * # running with scissoring on and EGL_SWAP_BEHAVIOUR set to EGL_BUFFER_PRESERVED
 * # running with scissoring on and EGL_SWAP_BEHAVIOUR set to EGL_BUFFER_DESTROYED
 * # running with scissoring off
 * Scissoring specifies a rectangle on screen, only ares inside that rectangle are then
 * affected by draw calls.
 * When EGL_SWAP_BEHAVIOUR is set to EGL_BUFFER_PRESERVED the contents of the color
 * buffer are preserved when eglSwapBuffers() is called.
 * When EGL_SWAP_BEHAVIOUR is set to EGL_BUFFER_DESTROYED the contents of the color
 * buffer could be destroyed or modified by eglSwapBuffers().
 * The effect is that in case 1 above the left half of the cube is preserved (not moving) 
 * in the color buffer while the right halve is updated (keeps spinning). 
 * In case 2 the left the left half of the screen is cleared and the right half is updated.
 */

#include "EGLPreserve.h"

#include "Text.h"
#include "Shader.h"
#include "Texture.h"
#include "Matrix.h"
#include "Platform.h"
#include "Timer.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>

#include <string>

#include "EGLRuntime.h"

#define WINDOW_W 800
#define WINDOW_H 600

using std::string;
using namespace MaliSDK;

/* Asset directories and filenames. */
string resourceDirectory = "assets/";
string vertexShaderFilename = "EGLPreserve_cube.vert";
string fragmentShaderFilename = "EGLPreserve_cube.frag";

string scissorOff = "Scissor:  off";
string scissorOn = "Scissor:  on ";
string preserveOff = "Preserve: off";
string preserveOn = "Preserve: on ";

/* Shader variables. */
GLuint programID = 0;
GLint iLocPosition = -1;
GLint iLocFillColor = -1;
GLint iLocProjection = -1;
GLint iLocModelview = -1;

/* Animation variables. */
Matrix perspective;
Matrix translation;

unsigned int windowWidth = 0;
unsigned int windowHeight = 0;

/* Timer variable to do the animation. */
Timer animationTimer;

/* A text object to draw text on the screen. */
Text *text;

bool setupGraphics(int width, int height)
{
    windowWidth = width;
    windowHeight = height;

    /* Full paths to the shader and texture files */
    string vertexShaderPath = resourceDirectory + vertexShaderFilename; 
    string fragmentShaderPath = resourceDirectory + fragmentShaderFilename;

    /* Initialize matrices. */
    perspective = Matrix::matrixPerspective(45.0f, windowWidth /(float)windowHeight, 0.01f, 100.0f);
    /* Move cube 2 further away from camera. */
    translation = Matrix::createTranslation(0.0f, 0.0f, -2.0f);

    /* Initialize OpenGL ES. */
    GL_CHECK(glEnable(GL_CULL_FACE));
    GL_CHECK(glCullFace(GL_BACK));
    GL_CHECK(glEnable(GL_DEPTH_TEST));
    GL_CHECK(glEnable(GL_BLEND));
    /* Should do src * (src alpha) + dest * (1-src alpha). */
    GL_CHECK(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

    /* Initialize the Text object. */
    text = new Text(resourceDirectory.c_str(), windowWidth, windowHeight);

    /* Process shaders. */
    GLuint vertexShaderID = 0;
    GLuint fragmentShaderID = 0;
    Shader::processShader(&vertexShaderID, vertexShaderPath.c_str(), GL_VERTEX_SHADER);
    Shader::processShader(&fragmentShaderID, fragmentShaderPath.c_str(), GL_FRAGMENT_SHADER);

    /* Set up shaders. */
    programID = GL_CHECK(glCreateProgram());
    GL_CHECK(glAttachShader(programID, vertexShaderID));
    GL_CHECK(glAttachShader(programID, fragmentShaderID));
    GL_CHECK(glLinkProgram(programID));
    GL_CHECK(glUseProgram(programID));

    /* Vertex positions. */
    iLocPosition = GL_CHECK(glGetAttribLocation(programID, "a_v4Position"));
    if(iLocPosition == -1)
    {
        LOGE("Attribute not found at %s:%i\n", __FILE__, __LINE__);
        return false;
    }
    GL_CHECK(glEnableVertexAttribArray(iLocPosition));

    /* Vertex colors. */
    iLocFillColor = GL_CHECK(glGetAttribLocation(programID, "a_v4FillColor"));
    if(iLocFillColor == -1)
    {
        LOGD("Warning: Attribute not found at %s:%i\n", __FILE__, __LINE__);
    }
    else 
    {
        GL_CHECK(glEnableVertexAttribArray(iLocFillColor));
    }

    /* Projection matrix. */
    iLocProjection = GL_CHECK(glGetUniformLocation(programID, "u_m4Projection"));
    if(iLocProjection == -1)
    {
        LOGD("Warning: Uniform not found at %s:%i\n", __FILE__, __LINE__);
    }
    else 
    {
        GL_CHECK(glUniformMatrix4fv(iLocProjection, 1, GL_FALSE, perspective.getAsArray()));
    }

    /* Modelview matrix. */
    iLocModelview = GL_CHECK(glGetUniformLocation(programID, "u_m4Modelview"));
    if(iLocModelview == -1)
    {
        LOGD("Warning: Uniform not found at %s:%i\n", __FILE__, __LINE__);
    }
    /* We pass this for each object, below. */

    /* Set clear screen color. */
    GL_CHECK(glClearColor(0.5f, 0.5f, 0.5f, 1.0));

    text->addString(windowWidth - scissorOff.length() * Text::textureCharacterWidth, Text::textureCharacterHeight, scissorOff.c_str(), 255, 0, 0, 255);

    /* Tweak EGL. */
    /* Check if the EGL_SWAP_BEHAVIOR_PRESERVED_BIT is set in EGL_SURFACE_TYPE. */
    EGLint swapBehaviour;
    if(eglQuerySurface(eglGetCurrentDisplay(), eglGetCurrentSurface(EGL_DRAW), EGL_SWAP_BEHAVIOR, &swapBehaviour) != EGL_TRUE)
    {
        LOGD("Warning: eglQuerySurface() failed at %s:%i\n", __FILE__, __LINE__);
    }

    LOGI("Default values:");
    LOGI("EGL_SWAP_BEHAVIOR = 0x%.4x", (int)swapBehaviour);
    switch(swapBehaviour)
    {
        case (EGL_BUFFER_PRESERVED):
            LOGI("                  = EGL_BUFFER_PRESERVED");
            break;
        case (EGL_BUFFER_DESTROYED):
            LOGI("                  = EGL_BUFFER_DESTROYED");
            break;
        default:
            LOGI("                  = UNKNOWN");
            break;
    }

    /* Set preserve bit. */
    if(eglSurfaceAttrib(eglGetCurrentDisplay(), eglGetCurrentSurface(EGL_DRAW), EGL_SWAP_BEHAVIOR, EGL_BUFFER_PRESERVED) != EGL_TRUE)
    {
        LOGD("Warning: eglSurfaceAttrib() failed at %s:%i\n", __FILE__, __LINE__);
    }
    
    return true;
}

void renderFrame(void)
{
    static float angleX = 0.0f;
    static float angleY = 0.0f;
    static float angleZ = 0.0f;

    static bool fullScreen = false;
    static bool preserve = false;

    /* Change the animation state if more than 3 seconds are passed since the last state change. */
    if(animationTimer.isTimePassed(3.0f))
    {
        fullScreen = !fullScreen;

        text->clear();
        if(fullScreen)
        {
            text->addString(windowWidth - scissorOff.length() * Text::textureCharacterWidth, Text::textureCharacterHeight, scissorOff.c_str(), 255, 0, 0, 255);
            LOGI("Scissor off\n");

            GL_CHECK(glDisable(GL_SCISSOR_TEST))
            preserve = !preserve;
            if(preserve)
            {
                /* Set preserve bit. */
                LOGI("Preserve on\n");
                if(eglSurfaceAttrib(eglGetCurrentDisplay(), eglGetCurrentSurface(EGL_DRAW), EGL_SWAP_BEHAVIOR, EGL_BUFFER_PRESERVED) != EGL_TRUE)
                {
                    LOGD("Warning: eglSurfaceAttrib() failed at %s:%i\n", __FILE__, __LINE__);
                }
            }
            else
            {
                /* Clear preserve bit. */
                LOGI("Preserve off\n");
                if(eglSurfaceAttrib(eglGetCurrentDisplay(), eglGetCurrentSurface(EGL_DRAW), EGL_SWAP_BEHAVIOR, EGL_BUFFER_DESTROYED) != EGL_TRUE)
                {
                    LOGD("Warning: eglSurfaceAttrib() failed at %s:%i\n", __FILE__, __LINE__);
                }
            }
        }
        else
        {
            text->addString(windowWidth - scissorOn.length() * Text::textureCharacterWidth, Text::textureCharacterHeight, scissorOn.c_str(), 0, 255, 0, 255);
            LOGI("Scissor on\n");
            GL_CHECK(glEnable(GL_SCISSOR_TEST));
            GL_CHECK(glScissor(windowWidth / 2, 0, windowWidth / 2, windowHeight));
            if(preserve)
            {
                text->addString(windowWidth - preserveOn.length() * Text::textureCharacterWidth, 0, preserveOn.c_str(), 0, 255, 0, 255);
            }
            else
            {
                text->addString(windowWidth - preserveOff.length() * Text::textureCharacterWidth, 0, preserveOff.c_str(), 255, 0, 0, 255);
            }
        }
    }

    GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

    Matrix rotationX = Matrix::createRotationX(angleX);
    Matrix rotationY = Matrix::createRotationY(angleY);
    Matrix rotationZ = Matrix::createRotationZ(angleZ);

    /* Update cube's rotation angles for animating. */
    angleX += 3;
    angleY += 2;
    angleZ += 1;

    if(angleX >= 360) angleX -= 360;
    if(angleY >= 360) angleY -= 360;
    if(angleZ >= 360) angleZ -= 360;

    /* Rotate about origin, then translate away from camera. */
    Matrix modelView = translation * rotationX;
    modelView = modelView * rotationY;
    modelView = modelView * rotationZ;

    GL_CHECK(glUseProgram(programID));

    GL_CHECK(glUniformMatrix4fv(iLocModelview, 1, GL_FALSE, modelView.getAsArray()));
    GL_CHECK(glUniformMatrix4fv(iLocProjection, 1, GL_FALSE, perspective.getAsArray()));

    /* Both drawing surfaces also share vertex data. */
    GL_CHECK(glEnableVertexAttribArray(iLocPosition));
    GL_CHECK(glVertexAttribPointer(iLocPosition, 3, GL_FLOAT, GL_FALSE, 0, cubeVertices));

    if(iLocFillColor != -1)
    {
        GL_CHECK(glEnableVertexAttribArray(iLocFillColor));
        GL_CHECK(glVertexAttribPointer(iLocFillColor, 4, GL_FLOAT, GL_FALSE, 0, cubeColors));
    }

    /* Draw the cube. */
    GL_CHECK(glDrawElements(GL_TRIANGLE_STRIP, 22, GL_UNSIGNED_BYTE, cubeIndices));

    /* Draw fonts. */
    text->draw();
}

int main(void)
{
    /* Intialize the Platform object for platform specific functions. */
    Platform *platform = Platform::getInstance();

    /* Initialize windowing system. */
    platform->createWindow(WINDOW_W, WINDOW_H);

    /* Initialize EGL. */
    EGLRuntime::initializeEGL(EGLRuntime::OPENGLES2);
    EGL_CHECK(eglMakeCurrent(EGLRuntime::display, EGLRuntime::surface, EGLRuntime::surface, EGLRuntime::context));

    /* Initialize OpenGL ES graphics subsystem. */
    setupGraphics(WINDOW_W, WINDOW_H);

    bool end = false;
    /* The rendering loop to draw the scene. */
    while(!end)
    {
        /* If something has happened to the window, end the sample. */
        if(platform->checkWindow() != Platform::WINDOW_IDLE)
        {
            end = true;
        }

        /* Render a single frame */
        renderFrame();

        /* 
         * Push the EGL surface color buffer to the native window.
         * Causes the rendered graphics to be displayed on screen.
         */
        eglSwapBuffers(EGLRuntime::display, EGLRuntime::surface);
    }

    /* Shut down OpenGL ES. */
    /* Shut down Text. */
    delete text;

    /* Shut down EGL. */
    EGLRuntime::terminateEGL();

    /* Shut down windowing system. */
    platform->destroyWindow();

    /* Shut down the platform object. */
    delete platform;

    return 0;
}

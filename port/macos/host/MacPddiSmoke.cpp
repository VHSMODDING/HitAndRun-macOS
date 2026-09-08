#include <pddi/pddi.hpp>
#include <pddi/gl/glmat.hpp>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>

int main()
{
    pddiDevice* device = nullptr;
    if (pddiCreate(PDDI_VERSION_MAJOR, PDDI_VERSION_MINOR, &device) != PDDI_OK || device == nullptr)
    {
        return 1;
    }

    pddiLibInfo info{};
    device->GetLibraryInfo(&info);
    if (info.libID != PDDI_LIBID_OPENGL) return 2;
    CGLPixelFormatAttribute attributes[] = {kCGLPFAAccelerated,
        static_cast<CGLPixelFormatAttribute>(0)};
    CGLPixelFormatObj format = nullptr;
    CGLContextObj context = nullptr;
    GLint count = 0;
    if (CGLChoosePixelFormat(attributes, &format, &count) != kCGLNoError || !format) return 3;
    CGLError error = CGLCreateContext(format, nullptr, &context);
    CGLDestroyPixelFormat(format);
    if (error != kCGLNoError || !context) return 4;
    CGLSetCurrentContext(context);
    int result = 0;
    {
        pglMat material(nullptr);
        const pddiBlendMode modes[] = {PDDI_BLEND_SUBTRACT, PDDI_BLEND_ALPHA,
            PDDI_BLEND_SUBMODULATEALPHA, PDDI_BLEND_NONE, PDDI_BLEND_ADD};
        for (auto mode : modes)
        {
            material.SetBlendMode(mode);
            material.SetPass(0);
            GLint equation = 0, source = 0, destination = 0;
            glGetIntegerv(GL_BLEND_EQUATION, &equation);
            glGetIntegerv(GL_BLEND_SRC, &source);
            glGetIntegerv(GL_BLEND_DST, &destination);
            const bool subtract = mode == PDDI_BLEND_SUBTRACT || mode == PDDI_BLEND_SUBMODULATEALPHA;
            if (equation != (subtract ? GL_FUNC_REVERSE_SUBTRACT : GL_FUNC_ADD)) result = 5;
            if (mode == PDDI_BLEND_SUBTRACT && (source != GL_ONE || destination != GL_ONE)) result = 6;
            if (glGetError() != GL_NO_ERROR) result = 7;
        }
    }
    CGLSetCurrentContext(nullptr);
    CGLDestroyContext(context);
    return result;
}

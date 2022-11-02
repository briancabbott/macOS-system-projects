
typealias CVOpenGLESTexture = CVImageBuffer
@available(iOS 5.0, *)
func CVOpenGLESTextureGetTypeID() -> CFTypeID
@available(iOS 5.0, *)
func CVOpenGLESTextureGetTarget(_ image: CVOpenGLESTexture) -> GLenum
@available(iOS 5.0, *)
func CVOpenGLESTextureGetName(_ image: CVOpenGLESTexture) -> GLuint
@available(iOS 5.0, *)
func CVOpenGLESTextureIsFlipped(_ image: CVOpenGLESTexture) -> Bool
@available(iOS 5.0, *)
func CVOpenGLESTextureGetCleanTexCoords(_ image: CVOpenGLESTexture, _ lowerLeft: UnsafeMutablePointer<GLfloat>, _ lowerRight: UnsafeMutablePointer<GLfloat>, _ upperRight: UnsafeMutablePointer<GLfloat>, _ upperLeft: UnsafeMutablePointer<GLfloat>)

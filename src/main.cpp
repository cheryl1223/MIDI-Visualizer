#include "Helpers.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <GLFW/glfw3.h>
#include <Eigen/Core>
#include <Eigen/Geometry>
#include <iostream>
#include <vector>
//#include <queue>
#include <iterator>

using namespace std;
using namespace Eigen;

MatrixXf model = MatrixXf::Identity(4,4);
MatrixXf view = MatrixXf::Identity(4,4);;
MatrixXf projection = MatrixXf::Identity(4,4);;
Vector3f cameraPos(0,0,10);
Vector3f cameraTarget(0,0,0);
Vector3f cameraFront(0,0,-1);
Vector3f cameraUp(0,1,0);

Vector3f rotation(0,0,0);
float angle_of_view = 60;
float aspect_ratio = 1;
float near = 0.1;
float far = 100.;
float gravity = -0.001;
bool pause = false;
float particle_size = 0.3;
float speed = 0.3;
float friction = 0.9;
int max_generators = 5;
int max_particles = 30;
int num_generators = 0;
//int num_particles = 0;

VertexBufferObject VBO;
VertexBufferObject VBO_C;
VertexBufferObject VBO_tex;

MatrixXf V = MatrixXf::Zero(3,6); //vertex position
MatrixXf C = MatrixXf::Zero(3,6); //per-vertex color
MatrixXf tex = MatrixXf::Zero(2,6); //per-vertex uv coordinates
unsigned int texture;
unsigned int particleTex;


Program program;
const GLchar* vertex_shader =
"#version 330 core\n"
"layout (location = 0) in vec3 aPos;"
"layout (location = 1) in vec3 aColor;"
"layout (location = 2) in vec2 aTexCoord;"

"out vec3 ourColor;"
"out vec2 TexCoord;"

"uniform mat4 model;"
"uniform mat4 view;"
"uniform mat4 projection;"

"void main()"
"{"
"gl_Position = projection * view * model * vec4(aPos, 1.0f);"
"ourColor = aColor;"
"TexCoord = vec2(aTexCoord.x, aTexCoord.y);"
"}";


const GLchar* fragment_shader =
"#version 330 core\n"
"out vec4 FragColor;"

"in vec3 ourColor;"
"in vec2 TexCoord;"

"uniform sampler2D texture1;"

"void main()"
"{"
"FragColor = texture(texture1, TexCoord)* vec4(ourColor, 1.0f);"
//"FragColor = vec4(ourColor, 1.0f);"
"}";

Matrix4f lookat(Vector3f cameraPos, Vector3f cameraTarget, Vector3f up) {
    Vector3f w = (cameraPos - cameraTarget).normalized();
    Vector3f u = up.cross(w).normalized();
    Vector3f v = w.cross(u);
    
    Matrix4f look;
    look <<
    u[0], u[1], u[2], 0.,
    v[0], v[1], v[2], 0.,
    w[0], w[1], w[2], 0.,
    0.,   0.,    0.,  1;
    
    Matrix4f at;
    at <<
    1, 0.0, 0.0, -cameraPos[0],
    0.0, 1, 0.0, -cameraPos[1],
    0.0, 0.0, 1, -cameraPos[2],
    0.0, 0.0, 0.0, 1;
    
    return look * at;
    
}

Matrix4f setPerspective(float angle_of_view, float far, float near) {
    Matrix4f M = Matrix4f::Identity();
    float theta = angle_of_view/2;
    float range = far - near;
    float invtan = 1./tan(theta);
    
    M(0,0) = invtan / aspect_ratio;
    M(1,1) = invtan;
    M(2,2) = -(near + far) / range;
    M(3,2) = -1;
    M(2,3) = -2 * near * far / range;
    M(3,3) = 0;
    return M;
    
    
}

Matrix4f setOrthographic(float angle_of_view, float far, float near) {
    Matrix4f M = Matrix4f::Identity();
    float length = (cameraPos - cameraTarget).norm();
    float h = tan(angle_of_view) * (length);
    float left = -h*aspect_ratio;
    float right = h*aspect_ratio;
    float top = h;
    float bottom = -h;
    M(0,0) = 2. / (right - left);
    M(1,1) = 2. / (top - bottom);
    M(2,2) = - 2./ (far - near);
    M(0,3) = - (right + left) / (right - left);
    M(1,3) = - (top + bottom) / (top - bottom);
    M(2,3) = - (far + near) / (far - near);
    return M;
}




void initialize() {
    V.col(0)<<
    0.5f,   0.5f, 0.0f,
    V.col(1)<<
    0.5f,  -0.5f, 0.0f,
    V.col(2)<<
    -0.5f,  0.5f, 0.0f,
    V.col(3)<<
    0.5f,  -0.5f, 0.0f,
    V.col(4)<<
    -0.5f, -0.5f, 0.0f,
    V.col(5)<<
    -0.5f,  0.5f, 0.0f;
    C.col(0)<<
    1.0f, 1.0f, 1.0f,
    C.col(1)<<
    1.0f, 1.0f, 1.0f,
    C.col(2)<<
    1.0f, 1.0f, 1.0f,
    C.col(3)<<
    1.0f, 1.0f, 1.0f,
    C.col(4)<<
    1.0f, 1.0f, 1.0f,
    C.col(5)<<
    1.0f, 1.0f, 1.0f;
    tex.col(0)<<
    1.0f, 1.0f,
    tex.col(1)<<
    1.0f, 0.0f,
    tex.col(2)<<
    0.0f, 1.0f,
    tex.col(3)<<
    1.0f, 0.0f,
    tex.col(4)<<
    0.0f, 0.0f,
    tex.col(5)<<
    0.0f, 1.0f;

    VBO.update(V);
    VBO_C.update(C);
    VBO_tex.update(tex);
    
}


unsigned int loadTexture(char const * path)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    
    int width, height, nrComponents;
    stbi_set_flip_vertically_on_load(true);
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;
        
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        stbi_image_free(data);
    }
    else
    {
        cout << "Texture failed to load at path: " << path << endl;
        stbi_image_free(data);
    }
    
    return textureID;
}



class Particle {
    
private:
    Vector3f position;
    Vector3f direction;
    Vector3f rotation;
    Vector3f color;
    float speed;
    float size;
    float life;
    
    
public:
    Particle(Vector3f position, Vector3f direction, Vector3f rotation,float speed, float size, Vector3f color) {
        
        this->life = 1000;
        
        //set the position, direction, and color
        this->size = size;
        this->speed = speed;
        this->position = position;
        this->direction = direction;
        this->rotation = rotation;
        this->color = color;
        
    }
    void updateLife(){
        this->life--;
    }
    
    //getters
    float getLife(){
        return this->life;
    }
    Vector3f getPosition(){
        return this->position;
    }
    Vector3f getDirection(){
        return this->direction;
    }
    Vector3f getColor(){
        return this->color;
    }
    Vector3f getRotation(){
        return this->rotation;
    }
    
    void setPosition(Vector3f newPosition){
        this->position = newPosition;
    }
    void setRotation(Vector3f newRotation){
        this->rotation = newRotation;
    }
    void setLife(int newLifeRemaining){
        this->life = newLifeRemaining;
    }
    
    void invertYDirection(){
        this->direction(1) *= -1;
    }
    void applyFriction(float friction){
        this->direction *= friction;
    }

    void applyGravity(float gravity){
        if (this->direction(1) > -0.3)
            this->direction(1) += 0.01*gravity*(1000-this->life);
    }
    
    void drawParticle() {
        for (int i = 0 ; i < 6; i++) {
            //V.col(i) += this->position;
            C.col(i) = this->color;
            
        }
        VBO_C.update(C);
        
        
        model = MatrixXf::Identity(4,4);
        Transform <float, 3, Affine > t;
        t = Transform <float , 3 , Affine >::Identity();
        t.scale(this->size);
        //t.rotate(this->rotation);
        t.translate(this->position);
        
        model = model*t.matrix();

        view = lookat(cameraPos, cameraTarget, cameraUp);
        projection = setPerspective(angle_of_view*M_PI/180, far, near)  ;
        glUniformMatrix4fv(program.uniform("model"), 1, GL_FALSE, model.data());
        glUniformMatrix4fv(program.uniform("view"), 1, GL_FALSE, view.data());
        glUniformMatrix4fv(program.uniform("projection"), 1, GL_FALSE, projection.data());

        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

};


class ParticleGenerator {
public:
    vector <Particle> particles;
    int num_particles = 0;
    int count = 0;
    ParticleGenerator(Vector3f position) {

        for (int i = 0; i < 3; i++) {
            this->position[i] = position[i];
        }
        
    }
    void addParticle() {
        Vector3f rotation(0,0,0);
        //random color
        Vector3f newColor( (float)(rand() / (float)RAND_MAX),
            (float)(rand() / (float)RAND_MAX),
            (float)(rand() / (float)RAND_MAX) );
        
        //random direction with bound
        Vector3f direction (0.1*(2.0f * ((float)rand() / (float)RAND_MAX) - 1.0f),
            -0.2f,
            0.1*(2.0f * ((float)rand() / (float)RAND_MAX) - 1.0f ));
        //cout<<direction<<endl;
        
        Particle particle = Particle(this->position,direction,rotation, speed, particle_size, newColor);
        particles.push_back(particle);
        num_particles ++;
        count ++;
        
    }
    
    void drawParticles() {
        float cur_time = glfwGetTime();
        if (!particles.empty()) {
            for (int i = 0; i < particles.size(); i++) {
                //cout<<i<<endl;
                particles[i].drawParticle();
            }
        }
    }
     
    void updateParticles(){
        Vector3f newPosition(0,0,0);
        Vector3f newRotation(0,0,0);
        for (auto p = particles.begin(); p < particles.end(); p++) {
            p->updateLife();
            if (p->getLife() <= 0) {
                num_particles --;
                particles.erase(p);
                continue;
            }
            p->applyGravity(gravity);

            for (int i = 0; i < 3; i++) {
                
                newPosition(i) = p->getPosition()(i);
                newPosition(i) += p->getDirection()(i) * speed ;
                
            }
            //cout<<p->getDirection()<<endl;
            if (newPosition(1) < 0.3) {
                p->invertYDirection();
                newPosition(1) = 0.3;
                //p->applyFriction(friction);
            }
            p->setPosition(newPosition);
            p->setRotation(newRotation);
            for (int i = 0; i < 3; i++)
                newPosition(i) = this->position(i);
            
        }
        
        
    }

    
    
private:
    Vector3f position;
};

Vector3f generator_positions[] = {
     Vector3f (0.0, 15.0, -10),
     Vector3f (5, 12, -10),
     Vector3f (3, -10, -10)
};
vector <ParticleGenerator> generators;
ParticleGenerator generator = ParticleGenerator(generator_positions[0]);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if(action == GLFW_RELEASE){
        switch (key)
        {
            case GLFW_KEY_SPACE:
                //cout<<"add"<<endl;
                max_particles += 10;
                cout<<"current max particle number is: "<<max_particles<<endl;

                break;
            case GLFW_KEY_1:
                particleTex = loadTexture("/Users/cheryl/cg/final-project-cheryl1223/src/1.png");
                break;
            case GLFW_KEY_2:
                particleTex = loadTexture("/Users/cheryl/cg/final-project-cheryl1223/src/2.png");
                break;
            case GLFW_KEY_3:
                particleTex = loadTexture("/Users/cheryl/cg/final-project-cheryl1223/src/3.png");
                break;
            case GLFW_KEY_4:
                particleTex = loadTexture("/Users/cheryl/cg/final-project-cheryl1223/src/4.png");
                break;
            case GLFW_KEY_5:
                particleTex = loadTexture("/Users/cheryl/cg/final-project-cheryl1223/src/5.png");
                break;

            case GLFW_KEY_W:
                cameraPos += 0.5 * cameraUp;
                break;
            case GLFW_KEY_A:
                cameraPos -= 0.5*(cameraFront.cross(cameraUp).normalized());
                break;
            case GLFW_KEY_S:
                cameraPos -= 0.5 * cameraUp;
                break;
            case GLFW_KEY_D:
                cameraPos += 0.5*(cameraFront.cross(cameraUp).normalized());
                break;
            case GLFW_KEY_Q:
                cameraPos -= 0.5 * cameraFront;
                break;
            case GLFW_KEY_E:
                cameraPos += 0.5 * cameraFront;
                break;
            default:
                break;
        }
        
    }
}
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    
    // Get the size of the window
    int width, height;
    glfwGetWindowSize(window, &width, &height);

    Vector4f p_screen(xpos,height-1-ypos,0,1);
    Vector4f p_canonical((p_screen[0]/width)*2-1,(p_screen[1]/height)*2-1,0,1);
    //cout<<view<<endl;
    //cout<<projection<<endl;
    //Vector4f p_world = view.inverse()*projection.inverse() *p_canonical;

    float xworld = p_canonical[0];
    float yworld = p_canonical[1];
    

    
    if (action == GLFW_RELEASE ){
        //cout<<xworld<<" "<<yworld<<endl;
        generators.push_back(ParticleGenerator(Vector3f(xworld*26,yworld*26,-10)));
        num_generators++;
    }
    
}

int main(){
    GLFWwindow* window;
    
    // Initialize the library
    if (!glfwInit())
        return -1;
    
    // Activate supersampling
    glfwWindowHint(GLFW_SAMPLES, 8);
    
    // Ensure that we get at least a 3.2 context
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    
    // On apple we have to load a core profile with forward compatibility
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    
    // Create a windowed mode window and its OpenGL context
    window = glfwCreateWindow(800, 800, "Hello World", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }
    
    // Make the window's context current
    glfwMakeContextCurrent(window);

    VertexArrayObject VAO;
    VAO.init();
    VAO.bind();

    VBO.init();
    VBO_C.init();
    VBO_tex.init();
    VBO.update(V);
    VBO_C.update(C);
    VBO_tex.update(tex);
    initialize();
    

    program.init(vertex_shader,fragment_shader,"FragColor");
    program.bind();
    program.bindVertexAttribArray("aPos",VBO);
    program.bindVertexAttribArray("aColor",VBO_C);
    program.bindVertexAttribArray("aTexCoord",VBO_tex);

    glfwSetKeyCallback(window, key_callback);
    //glfwSetCursorPosCallback(window, cursor_pos_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    //glfwSetWindowSizeCallback(window, window_size_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // tell GLFW to capture our mouse
    //glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
 

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    particleTex = loadTexture("/Users/cheryl/cg/final-project-cheryl1223/src/1.png");
    
    while (!glfwWindowShouldClose(window))
    {

        //glEnable(GL_STENCIL_TEST);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        //glBlendFunc(GL_SRC_ALPHA, GL_ONE);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        //glEnable(GL_DEPTH_TEST);
        //glDepthFunc(GL_LESS);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        VAO.bind();
        program.bind();
        glBindTexture(GL_TEXTURE_2D, particleTex);
        
        for (auto i = generators.begin(); i < generators.end(); i++) {
            if (i->count < max_particles)
                i->addParticle();
            i->updateParticles();
            if (i->num_particles == 0) {
                generators.erase(i);
                num_generators--;
            }
        }
        
        if (num_generators > 0) {
            for (int i = 0 ; i < num_generators; i++) {
                generators[i].drawParticles();
            }
        }


        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    program.free();
    VAO.free();
    VBO.free();
    glfwTerminate();
    return 0;
}
    




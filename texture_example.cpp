#include <gtkmm.h>
#include <epoxy/gl.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

const char *vertex_shader_src = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
out vec2 TexCoord;
void main() {
  gl_Position = vec4(aPos, 0.0, 1.0);
  TexCoord = vec2(aTexCoord.x, 1.0 - aTexCoord.y); // Flip texture vertically
}
)";

const char *fragment_shader_src = R"(
#version 330 core
out vec4 FragColor;
in vec2 TexCoord;
uniform sampler2D texture1;
void main() {
  FragColor = texture(texture1, TexCoord);
}
)";

GLuint compile_shader(GLenum type, const char *src) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &src, NULL);
  glCompileShader(shader);
  GLint success;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char infoLog[512];
    glGetShaderInfoLog(shader, 512, NULL, infoLog);
    g_warning("Shader compilation failed: %s", infoLog);
  }
  return shader;
}

GLuint create_shader_program() {
  GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_shader_src);
  GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_src);
  GLuint shader_program = glCreateProgram();
  glAttachShader(shader_program, vertex_shader);
  glAttachShader(shader_program, fragment_shader);
  glLinkProgram(shader_program);
  GLint success;
  glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
  if (!success) {
    char infoLog[512];
    glGetProgramInfoLog(shader_program, 512, NULL, infoLog);
    g_warning("Shader program linking failed: %s", infoLog);
  }
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  return shader_program;
}

class MyGLArea : public Gtk::GLArea {
public:
  MyGLArea() : shader_program(0), texture(0), VAO(0), VBO(0), EBO(0), initialized(false) {
    set_required_version(3, 3);
    signal_realize().connect(sigc::mem_fun(*this, &MyGLArea::on_realize));
    signal_unrealize().connect(sigc::mem_fun(*this, &MyGLArea::on_unrealize), false);
    signal_render().connect(sigc::mem_fun(*this, &MyGLArea::on_render));
  }

protected:
  void on_realize() override {
    Gtk::GLArea::on_realize();
    make_current();
    if (glGetError() != GL_NO_ERROR) {
      g_warning("Failed to initialize OpenGL context");
      return;
    }

    init_buffers();
    shader_program = create_shader_program();
    initialized = true;
  }

  void on_unrealize() override {
    make_current();

    if (initialized) {
      glDeleteBuffers(1, &VBO);
      glDeleteBuffers(1, &EBO);
      glDeleteVertexArrays(1, &VAO);
      glDeleteTextures(1, &texture);
      glDeleteProgram(shader_program);
      initialized = false;
    }

    Gtk::GLArea::on_unrealize();
  }

  bool on_render(const Glib::RefPtr<Gdk::GLContext>& context) override {
    if (!initialized) {
      return false;
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(shader_program);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    return true;
  }

  void init_buffers() {
    // Load texture
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, channels;
    unsigned char *image = stbi_load("GTK.png", &width, &height, &channels, 4);
    if (image) {
      g_print("Image loaded: %dx%d, %d channels\n", width, height, channels);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
      glGenerateMipmap(GL_TEXTURE_2D);
      stbi_image_free(image);
    } else {
      g_warning("Failed to load texture");
    }

    // Set up vertex data
    float vertices[] = {
      // positions    // texture coords
      -1.0f, -1.0f,  0.0f, 0.0f,
       1.0f, -1.0f,  1.0f, 0.0f,
       1.0f,  1.0f,  1.0f, 1.0f,
      -1.0f,  1.0f,  0.0f, 1.0f
    };
    unsigned int indices[] = {
      0, 1, 2,
      2, 3, 0
    };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
  }

private:
  GLuint shader_program;
  GLuint texture;
  GLuint VAO, VBO, EBO;
  bool initialized;
};

int main(int argc, char *argv[]) {
  auto app = Gtk::Application::create("org.gtkmm.example");
  Gtk::Window window;
  MyGLArea glarea;
  window.set_default_size(800, 600);
  window.add(glarea);
  window.show_all(); // Ensure window is shown
  return app->run(window);
}

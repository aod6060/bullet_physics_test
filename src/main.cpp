/*
Copyright 2019 Frederick R. Cook

Permission is hereby granted, free of charge, to any person obtaining a copy 
of this software and associated documentation files (the "Software"), to deal 
in the Software without restriction, including without limitation the rights to 
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies 
of the Software, and to permit persons to whom the Software is furnished to do 
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all 
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS 
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR 
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN 
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION 
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <functional>

#include <SDL.h>
#include <SDL_image.h>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <btBulletDynamicsCommon.h>

#define BIT(v) (1<<v)

#define COL_NONE 0x0
#define COL_OBJECT BIT(1)
#define COL_CAMERA BIT(2)
#define COL_GROUND BIT(3)
#define COL_EVERYTHING -1

#define FIXED_FRAME_60 (1.0f / 60.0f)

static std::string g_caption = "Bullet Physics Test";

static uint32_t g_width = 1280;
static uint32_t g_height = 720;

static bool g_running = true;

static SDL_Window* g_window = nullptr;
static SDL_GLContext g_context = nullptr;

static uint32_t g_preTime = 0;
static uint32_t g_currTime = 0;
static float g_delta = 0.0f;
static float g_fixedTime = 0.0f;

static SDL_Event g_event;

void app_init();
void app_event(SDL_Event& e);
void app_update(float delta);
void app_fixedUpdate();
void app_render();
void app_release();

int main(int argc, char** argv) {

	SDL_Init(SDL_INIT_EVERYTHING);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	g_window = SDL_CreateWindow(
		g_caption.c_str(),
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		g_width,
		g_height,
		SDL_WINDOW_OPENGL
	);


	g_context = SDL_GL_CreateContext(g_window);

	glewInit();

	app_init();

	g_preTime = SDL_GetTicks();

	while (g_running) {

		g_currTime = SDL_GetTicks();
		g_delta = (g_currTime - g_preTime) / 1000.0f;
		g_fixedTime += g_delta;
		g_preTime = g_currTime;

		while (SDL_PollEvent(&g_event)) {
			if (g_event.type == SDL_QUIT) {
				g_running = false;
			}

			app_event(g_event);
		}


		app_update(g_delta);
		
		if (g_fixedTime >= FIXED_FRAME_60) {
			app_fixedUpdate();
		}

		app_render();

		if (g_fixedTime >= FIXED_FRAME_60) {
			g_fixedTime = 0.0f;
		}

		SDL_GL_SwapWindow(g_window);
	}

	app_release();

	SDL_GL_DeleteContext(g_context);
	SDL_DestroyWindow(g_window);

	SDL_Quit();

	return 0;
}

/*
	------------------ App Section --------------------------
*/
struct Shader {
	uint32_t id;

	void init(GLenum type, std::string path) {
		id = glCreateShader(type);

		std::ifstream in(path);

		std::stringstream ss;
		std::string temp;

		while (std::getline(in, temp)) {
			ss << temp << std::endl;
		}

		in.close();

		std::string src = ss.str();

		const char* c_src = src.c_str();
		std::cout << c_src << std::endl;

		//std::cout << c_src << std::endl;

		glShaderSource(id, 1, &c_src, 0);
		glCompileShader(id);

		char log[1024];
		int len;

		glGetShaderiv(id, GL_INFO_LOG_LENGTH, &len);

		if (len > 0) {
			glGetShaderInfoLog(id, len, 0, log);
			std::cout << log << std::endl;
		}
	}

	void release() {
		glDeleteShader(this->id);
	}

};

struct Program {
	uint32_t programID = 0;
	// Shaders
	std::vector<Shader*> shaders;
	// Attributes
	uint32_t attributeID = 0;
	std::map<std::string, uint32_t> attributeMapping;
	// Uniforms
	std::map<std::string, uint32_t> uniformsMapping;

	// Program Section
	void init() {
		// Create Program and Uploading shaders
		this->programID = glCreateProgram();
		std::for_each(shaders.begin(), shaders.end(), [&](Shader* shader) {
			glAttachShader(this->programID, shader->id);
		});
		glLinkProgram(this->programID);

		int len;
		char log[1024];

		glGetProgramiv(programID, GL_INFO_LOG_LENGTH, &len);

		if (len > 0) {
			glGetProgramInfoLog(programID, len, 0, log);
			std::cout << log << std::endl;
		}

		// Setting Up Attributes
		glGenVertexArrays(1, &this->attributeID);
	}

	void bind() {
		glUseProgram(this->programID);
	}

	void unbind() {
		glUseProgram(0);
	}

	void release() {
		// Delete Vertex Array
		glDeleteVertexArrays(1, &this->attributeID);

		std::for_each(shaders.begin(), shaders.end(), [&](Shader* shader) {
			glDetachShader(this->programID, shader->id);
		});
	}

	// Shader
	void addShader(Shader* shader) {
		this->shaders.push_back(shader);
	}

	// Uniform
	void createUniform(std::string name) {
		this->uniformsMapping[name] = glGetUniformLocation(this->programID, name.c_str());
	}

	void set1i(std::string name, int x) {
		glUniform1i(this->uniformsMapping[name], x);
	}

	void set2i(std::string name, const glm::ivec2& v) {
		glUniform2i(this->uniformsMapping[name], v.x, v.y);
	}

	void set3i(std::string name, const glm::ivec3& v) {
		glUniform3i(this->uniformsMapping[name], v.x, v.y, v.z);
	}

	void set4i(std::string name, const glm::ivec4& v) {
		glUniform4i(this->uniformsMapping[name], v.x, v.y, v.z, v.w);
	}

	void set1f(std::string name, float x) {
		glUniform1f(this->uniformsMapping[name], x);
	}

	void set2f(std::string name, const glm::vec2& v) {
		glUniform2f(this->uniformsMapping[name], v.x, v.y);
	}

	void set3f(std::string name, const glm::vec3& v) {
		glUniform3f(this->uniformsMapping[name], v.x, v.y, v.z);
	}

	void set4f(std::string name, const glm::vec4& v) {
		glUniform4f(this->uniformsMapping[name], v.x, v.y, v.z, v.w);
	}

	void setMat2(std::string name, const glm::mat2& m) {
		glUniformMatrix2fv(this->uniformsMapping[name], 1, GL_FALSE, &m[0][0]);
	}

	void setMat3(std::string name, const glm::mat3& m) {
		glUniformMatrix3fv(this->uniformsMapping[name], 1, GL_FALSE, &m[0][0]);
	}

	void setMat4(std::string name, const glm::mat4& m) {
		glUniformMatrix4fv(this->uniformsMapping[name], 1, GL_FALSE, &m[0][0]);
	}

	// Attribute
	void setAttribute(std::string name, uint32_t id) {
		this->attributeMapping[name] = id;
	}

	void enableAttribute(std::string name) {
		glEnableVertexAttribArray(this->attributeMapping[name]);
	}

	void disableAttribute(std::string name) {
		glDisableVertexAttribArray(this->attributeMapping[name]);
	}

	void pointerAttribute(
		std::string name,
		uint32_t size,
		GLenum type) {
		glVertexAttribPointer(
			this->attributeMapping[name],
			size,
			type,
			GL_FALSE,
			0,
			0
		);
	}

	void bindAttribute() {
		glBindVertexArray(this->attributeID);
	}

	void unbindAttribute() {
		glBindVertexArray(0);
	}

};

struct VertexBuffer {
	uint32_t id = 0;
	std::vector<float> list;
	bool isStatic = true;

	void add(float x) {
		list.push_back(x);
	}

	void add(float x, float y) {
		list.push_back(x);
		list.push_back(y);
	}

	void add(float x, float y, float z) {
		list.push_back(x);
		list.push_back(y);
		list.push_back(z);
	}

	void add(float x, float y, float z, float w) {
		list.push_back(x);
		list.push_back(y);
		list.push_back(z);
		list.push_back(w);
	}

	void clear() {
		list.clear();
	}

	void init(bool isStatic = true) {
		glGenBuffers(1, &this->id);
		this->isStatic = isStatic;
	}

	void upload() {
		this->bind();
		glBufferData(GL_ARRAY_BUFFER, this->size() * sizeof(float), list.data(), (this->isStatic) ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW);
		this->unbind();
	}

	void bind() {
		glBindBuffer(GL_ARRAY_BUFFER, id);
	}

	void unbind() {
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	void release() {
		this->clear();
		glDeleteBuffers(1, &id);
	}

	uint32_t size() {
		return list.size();
	}
};

struct IndexBuffer {
	uint32_t id;
	std::vector<uint32_t> list;

	void add(uint32_t x) {
		list.push_back(x);
	}

	void add(uint32_t x, uint32_t y) {
		list.push_back(x);
		list.push_back(y);
	}

	void add(uint32_t x, uint32_t y, uint32_t z) {
		list.push_back(x);
		list.push_back(y);
		list.push_back(z);
	}

	void add(uint32_t x, uint32_t y, uint32_t z, uint32_t w) {
		list.push_back(x);
		list.push_back(y);
		list.push_back(z);
		list.push_back(w);
	}

	void clear() {
		list.clear();
	}

	void init() {
		glGenBuffers(1, &id);
	}

	void upload() {
		bind();
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, list.size() * sizeof(uint32_t), list.data(), GL_DYNAMIC_DRAW);
		unbind();
	}

	void bind() {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id);
	}

	void unbind() {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}

	void release() {
		clear();
		glDeleteBuffers(1, &id);
	}

	uint32_t size() {
		return list.size();
	}

};

struct IGeometry {
	virtual void init() = 0;
	virtual void render(Program& program) = 0;
	virtual void release() = 0;
};

struct GeometryPlane : public IGeometry {

	VertexBuffer vertices;
	IndexBuffer indincies;

	virtual void init() {

		// Vertices
		this->vertices.init();
		this->vertices.add(1.0f, 0.0f, -1.0f);
		this->vertices.add(1.0f, 0.0f, 1.0f);
		this->vertices.add(-1.0f, 0.0f, -1.0f);
		this->vertices.add(-1.0f, 0.0f, 1.0f);
		this->vertices.upload();

		// Indencies
		this->indincies.init();
		this->indincies.add(0, 1, 2);
		this->indincies.add(2, 1, 3);
		this->indincies.upload();
	}

	virtual void render(Program& program) {
		program.bindAttribute();

		vertices.bind();
		program.pointerAttribute("vertices", 3, GL_FLOAT);
		vertices.unbind();


		indincies.bind();
		glDrawElements(GL_TRIANGLES, indincies.size(), GL_UNSIGNED_INT, 0);
		indincies.unbind();

		program.unbindAttribute();
	}

	virtual void release() {
		indincies.release();
		vertices.release();
	}

};

struct GeometryCube : public IGeometry {
	VertexBuffer vertices;
	IndexBuffer indincies;

	virtual void init() {
		vertices.init();

		// 0
		vertices.add(-1.0f, 1.0f, -1.0f);
		// 1
		vertices.add(1.0f, 1.0f, -1.0f);
		// 2
		vertices.add(-1.0f, -1.0f, -1.0f);
		// 3
		vertices.add(1.0f, -1.0f, -1.0f);
		// 4
		vertices.add(-1.0f, 1.0f, 1.0f);
		// 5
		vertices.add(1.0f, 1.0f, 1.0f);
		// 6
		vertices.add(-1.0f, -1.0f, 1.0f);
		// 7
		vertices.add(1.0f, -1.0f, 1.0f);

		vertices.upload();


		indincies.init();
		// left
		indincies.add(0, 2, 4);
		indincies.add(4, 2, 6);
		// right
		indincies.add(1, 3, 5);
		indincies.add(5, 3, 7);
		// top
		indincies.add(0, 1, 4);
		indincies.add(4, 1, 5);
		// bottom
		indincies.add(2, 3, 6);
		indincies.add(6, 3, 7);
		// front
		indincies.add(4, 5, 6);
		indincies.add(6, 5, 7);
		// back
		indincies.add(0, 1, 2);
		indincies.add(2, 1, 3);
		indincies.upload();
	}

	virtual void render(Program& program) {
		program.bindAttribute();

		vertices.bind();
		program.pointerAttribute("vertices", 3, GL_FLOAT);
		vertices.unbind();

		indincies.bind();
		glDrawElements(GL_TRIANGLES, indincies.size(), GL_UNSIGNED_INT, 0);
		indincies.unbind();

		program.unbindAttribute();
	}

	virtual void release() {
		indincies.release();
		vertices.release();
	}

};

struct GeometrySphere {

	VertexBuffer vertices;
	IndexBuffer indincies;

	virtual void init() {

		int count = 32;

		float PI = 3.14159f;
		float p = 1.0f;
		float slice = 360.0f / (float)count;
		float slice2 = 180.0f / (float)(count / 2);

		vertices.init();

		for (float phi = 0.0f; phi <= 180.0f; phi += slice2) {
			for (float pheta = 0.0f; pheta < 360.0f; pheta += slice) {
				float rphi = glm::radians(phi);
				float rpheta = glm::radians(pheta);

				float x = p * sin(rphi) * cos(rpheta);
				float y = p * sin(rphi) * sin(rpheta);
				float z = p * cos(rphi);

				vertices.add(x, y, z);
			}
		}
		vertices.upload();

		indincies.init();
		for (int y = 0; y < count / 2; y++) {
			for (int x = 0; x < count; x++) {
				int p0 = y * count + x;
				int p1 = y * count + (x + 1);
				int p2 = (y + 1) * count + x;
				int p3 = (y + 1) * count + (x + 1);

				indincies.add(p0, p1, p2);
				indincies.add(p2, p1, p3);
			}
		}
		indincies.upload();

	}

	virtual void render(Program& program) {
		program.bindAttribute();

		vertices.bind();
		program.pointerAttribute("vertices", 3, GL_FLOAT);
		vertices.unbind();

		indincies.bind();
		glDrawElements(GL_TRIANGLES, indincies.size(), GL_UNSIGNED_INT, 0);
		indincies.unbind();

		program.unbindAttribute();
	}

	virtual void release() {
		indincies.release();
		vertices.release();
	}

};

struct GeometryQuad : public IGeometry {

	VertexBuffer vertices;
	VertexBuffer texCoords;
	IndexBuffer index;

	virtual void init() {
		vertices.init();
		vertices.add(-1.0f, 1.0f, 0.0f);
		vertices.add(1.0f, 1.0f, 0.0f);
		vertices.add(-1.0f, -1.0f, 0.0f);
		vertices.add(1.0f, -1.0f, 0.0f);
		vertices.upload();

		texCoords.init();
		texCoords.add(0.0f, 0.0f);
		texCoords.add(1.0f, 0.0f);
		texCoords.add(0.0f, 1.0f);
		texCoords.add(1.0f, 1.0f);
		texCoords.upload();

		index.init();
		index.add(0, 1, 2);
		index.add(2, 1, 3);
		index.upload();
	}

	virtual void render(Program& program) {
		program.bindAttribute();

		vertices.bind();
		program.pointerAttribute("vertices", 3, GL_FLOAT);
		vertices.unbind();

		texCoords.bind();
		program.pointerAttribute("texCoords", 2, GL_FLOAT);
		texCoords.unbind();

		index.bind();
		glDrawElements(GL_TRIANGLES, index.size(), GL_UNSIGNED_INT, 0);
		index.unbind();

		program.unbindAttribute();
	}
	
	virtual void release() {
		index.release();
		texCoords.release();
		vertices.release();
	}

};

struct Texture2D {
	uint32_t id = 0;
	uint32_t width = 0;
	uint32_t height = 0;

	void init(std::string path) {
		SDL_Surface* surf = IMG_Load(path.c_str());

		if (surf == nullptr) {
			std::cout << path << "doesn't exist..." << std::endl;
			return;
		}

		width = surf->w;
		height = surf->h;

		if (id == 0) {
			glGenTextures(1, &this->id);
		}

		uint32_t type = GL_RGB;

		if (surf->format->BytesPerPixel == 4) {
			type = GL_RGBA;
		}

		glBindTexture(GL_TEXTURE_2D, id);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

		glTexImage2D(GL_TEXTURE_2D, 0, type, this->width, this->height, 0, type, GL_UNSIGNED_BYTE, surf->pixels);

		glBindTexture(GL_TEXTURE_2D, 0);

		SDL_FreeSurface(surf);


	}

	void bind(uint32_t texture = GL_TEXTURE0) {
		glActiveTexture(texture);
		glBindTexture(GL_TEXTURE_2D, id);
	}

	void unbind(uint32_t texture = GL_TEXTURE0) {
		glActiveTexture(texture);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void release() {
		glDeleteTextures(1, &this->id);
	}

};

struct PhysicsObject {
	btRigidBody* body;
	int group;
	int mask;
};

struct Physics {
	btBroadphaseInterface* broadphase;
	btCollisionDispatcher* disp;
	btConstraintSolver* solver;
	btDefaultCollisionConfiguration* collisionConf;
	btDiscreteDynamicsWorld* dynamicWorld;

	std::vector<PhysicsObject> physicsObjects;

	void init() {
		this->collisionConf = new btDefaultCollisionConfiguration();
		this->disp = new btCollisionDispatcher(this->collisionConf);
		this->broadphase = new btDbvtBroadphase();
		this->solver = new btSequentialImpulseConstraintSolver;
		this->dynamicWorld = new btDiscreteDynamicsWorld(
			disp,
			broadphase,
			this->solver,
			this->collisionConf
		);

		dynamicWorld->setGravity(btVector3(0, -10, 0));
	}

	btDiscreteDynamicsWorld* getWorld() { return this->dynamicWorld; }

	void stepSimulation() {
		this->getWorld()->stepSimulation(FIXED_FRAME_60);
	}

	btBoxShape* createBoxShape(const btVector3& halfExtents) {
		btBoxShape* box = new btBoxShape(halfExtents);
		return box;
	}

	btSphereShape* createSphereShape(btScalar scalar) {
		btSphereShape* sphere = new btSphereShape(scalar);
		return sphere;
	}

	btStaticPlaneShape* createStaticPlaneShape(const btVector3& planeNormal, btScalar planeConstant) {

		btStaticPlaneShape* plane = new btStaticPlaneShape(planeNormal, planeConstant);
		return plane;
	}

	btCapsuleShape* createCapsuleShape(btScalar radius, btScalar height) {
		btCapsuleShape* capsule = new btCapsuleShape(radius, height);
		return capsule;
	}

	btRigidBody* createRigid(float mass, const btTransform& startTransform, btCollisionShape* shape) {
		bool isDynamic = (mass != 0.f);

		btVector3 localInertial(0, 0, 0);

		if (isDynamic) {
			shape->calculateLocalInertia(mass, localInertial);
		}

		btDefaultMotionState* ms = new btDefaultMotionState(startTransform);

		btRigidBody::btRigidBodyConstructionInfo cinfo(mass, ms, shape, localInertial);

		btRigidBody* body = new btRigidBody(cinfo);

		body->setUserIndex(-1);

		PhysicsObject po = {
			body,
			-1,
			-1
		};

		physicsObjects.push_back(po);

		this->getWorld()->addRigidBody(po.body);
		//this->rigidBodies.push_back(body);

		return body;
	}

	btRigidBody* createRigid(float mass, const btTransform& startTransform, btCollisionShape* shape, int collisionFilterGroup, int mask) {
		bool isDynamic = (mass != 0.f);

		btVector3 localInertial(0, 0, 0);

		if (isDynamic) {
			shape->calculateLocalInertia(mass, localInertial);
		}

		btDefaultMotionState* ms = new btDefaultMotionState(startTransform);

		btRigidBody::btRigidBodyConstructionInfo cinfo(mass, ms, shape, localInertial);

		btRigidBody* body = new btRigidBody(cinfo);

		//body->setUserIndex(-1);
		PhysicsObject po = {
			body,
			collisionFilterGroup,
			mask
		};

		physicsObjects.push_back(po);

		this->getWorld()->addRigidBody(body,collisionFilterGroup, mask);

		return body;
	}

	void removeRigidBody(btRigidBody* body) {
		int i = 0;
		for (; i < physicsObjects.size(); i++) {
			if (body == physicsObjects[i].body) {
				break;
			}
		}

		physicsObjects.erase(physicsObjects.begin() + i);

		getWorld()->removeRigidBody(body);
		btMotionState* ms = body->getMotionState();
		delete body;
		delete ms;
	}

	void release() {
		delete this->dynamicWorld;
		delete this->solver;
		delete this->broadphase;
		delete this->disp;
		delete this->collisionConf;
	}

	void getRigidBodiesFromAABB(const btVector3& minAABB, const btVector3& maxAABB, std::vector<btRigidBody*>& rigidBodies, int mask) {

		std::function<bool(const btVector3&, const btVector3&, const btVector3&)> pointInAABB = [](const btVector3& minAABB, const btVector3& maxAABB, const btVector3& point) {

			return 
				minAABB.x() <= point.x() &&
				minAABB.y() <= point.y() &&
				minAABB.z() <= point.z() &&
				maxAABB.x() >= point.x() &&
				maxAABB.y() >= point.y() &&
				maxAABB.z() >= point.z();
		};

		//std::cout << physicsObjects.size() << std::endl;

		for (int i = 0; i < physicsObjects.size(); i++) {
			if (physicsObjects[i].group == mask) {
				btVector3 point = physicsObjects[i].body->getCenterOfMassPosition();
				if (pointInAABB(minAABB, maxAABB, point)) {
					rigidBodies.push_back(physicsObjects[i].body);
				}
			}
		}
	}
};

struct Camera {
	glm::vec3 pos;
	glm::vec2 rot;

	float fov;
	float aspect;
	float znear;
	float zfar;

	float speed = 64.0f;
	float walkSpeed = 32.0f;

	void init(
		glm::vec3 pos,
		glm::vec2 rot,
		float fov,
		float aspect,
		float znear,
		float zfar) {
		this->pos = pos;
		this->rot = rot;
		this->fov = fov;
		this->aspect = aspect;
		this->znear = znear;
		this->zfar = zfar;

		SDL_SetRelativeMouseMode(SDL_TRUE);
	}

	void update(float delta) {
		int x = 0, y = 0;
		uint32_t buttons = SDL_GetRelativeMouseState(&x, &y);
		const uint8_t* keys = SDL_GetKeyboardState(nullptr);

		rot.x += this->speed * y * ((delta - 0.001f < 0) ? 0.001f : delta);
		rot.y += this->speed * x * ((delta - 0.001f < 0) ? 0.001f : delta);

		if (rot.y <= -360.0f) {
			rot.y += 360.0f;
		}

		if (rot.y >= 360.0f) {
			rot.y -= 360.0f;
		}

		if (rot.x < -90.0f) {
			rot.x = -90.0f;
		}

		if (rot.x > 90.0f) {
			rot.x = 90.0f;
		}

		float yrad = glm::radians(rot.y);

		float sp = this->walkSpeed;

		if (keys[SDL_SCANCODE_E]) {
			sp *= 3.0f;
		}

		if (keys[SDL_SCANCODE_W]) {
			pos.x += sp * glm::sin(yrad) * delta;
			pos.z -= sp * glm::cos(yrad) * delta;
		}

		if (keys[SDL_SCANCODE_S]) {
			pos.x -= sp * glm::sin(yrad) * delta;
			pos.z += sp * glm::cos(yrad) * delta;
		}

		if (keys[SDL_SCANCODE_A]) {
			pos.x -= sp * glm::cos(yrad) * delta;
			pos.z -= sp * glm::sin(yrad) * delta;
		}

		if (keys[SDL_SCANCODE_D]) {
			pos.x += sp * glm::cos(yrad) * delta;
			pos.z += sp * glm::sin(yrad) * delta;
		}

		if (keys[SDL_SCANCODE_LSHIFT]) {
			pos.y -= sp * delta;
		}

		if (keys[SDL_SCANCODE_SPACE]) {
			pos.y += sp * delta;
		}
	}

	glm::mat4 getProjection() {
		return glm::perspective(
			glm::radians(this->fov),
			aspect,
			znear,
			zfar);
	}

	glm::mat4 getView() {
		return
			glm::rotate(glm::mat4(1.0f), glm::radians(this->rot.x), glm::vec3(1.0f, 0.0f, 0.0f)) *
			glm::rotate(glm::mat4(1.0f), glm::radians(this->rot.y), glm::vec3(0.0f, 1.0f, 0.0f)) *
			glm::translate(glm::mat4(1.0f), -this->pos);
	}
};

struct DebugLine {
	VertexBuffer buffer;

	glm::vec3 from;
	glm::vec3 to;

	GeometrySphere sphere;

	void init() {
		buffer.init(true);

		sphere.init();
	}

	void render(Program& program) {
		if (buffer.size() > 0) {
			glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));

			program.setMat4("model", model);
			program.set4f("frag_Color", glm::vec4(1.0f, 0.0f, 1.0f, 1.0f));

			program.bindAttribute();

			buffer.bind();
			program.pointerAttribute("vertices", 3, GL_FLOAT);
			buffer.unbind();

			glDrawArrays(GL_LINES, 0, buffer.size() / 2);

			program.unbindAttribute();

			model = 
				glm::translate(glm::mat4(1.0f), this->from) * 
				glm::scale(glm::mat4(1.0f), glm::vec3(0.25f));

			program.setMat4("model", model);
			sphere.render(program);

			model =
				glm::translate(glm::mat4(1.0f), this->to) *
				glm::scale(glm::mat4(1.0f), glm::vec3(0.25f));

			program.setMat4("model", model);
			sphere.render(program);


		}
	}

	void release() {
		sphere.release();
		buffer.release();
	}

	void setLine(btVector3 from, btVector3 to) {
		this->from = this->covert(from);
		this->to = this->covert(to);

		buffer.clear();
		buffer.add(from.x(), from.y(), from.z());
		buffer.add(to.x(), to.y(), to.z());
		buffer.upload();
	}

	glm::vec3 covert(const btVector3& v) {
		return glm::vec3(v.x(), v.y(), v.z());
	}
};

// Main Shader
static Shader vertexShader;
static Shader fragmentShader;
static Program program;

static Shader hubVertexShader;
static Shader hubFragmentShader;
static Program hubProgram;

static Physics physics;
static DebugLine debugLine;

//static Camera camera;


struct FloorObject {
	GeometryPlane floor;
	btRigidBody* body;
	btCollisionShape* shape;

	void init() {
		floor.init();

		shape = physics.createStaticPlaneShape(btVector3(0, 1, 0), 0);

		btTransform transform = btTransform(btQuaternion(0, 0, 0, 1));

		body = physics.createRigid(0, transform, shape, COL_GROUND, COL_EVERYTHING);
	}

	void render() {
		btTransform transform = body->getWorldTransform();
		btScalar m[16];
		transform.getOpenGLMatrix(m);

		glm::mat4 model = 
			glm::make_mat4(m) * 
			glm::scale(glm::mat4(1.0f), glm::vec3(20.0f, 0.0f, 20.0f));

		program.setMat4("model", model);
		program.set4f("frag_Color", glm::vec4(0.0f, 0.5f, 0.0f, 1.0f));

		floor.render(program);

	}

	void release() {
		physics.removeRigidBody(body);
		delete shape;
		floor.release();
	}
};

struct BoxObject {
	GeometryCube box;
	btRigidBody* body;
	btCollisionShape* shape;

	void init(const btQuaternion& rotation, const btVector3& position) {
		box.init();
		shape = physics.createBoxShape(btVector3(1, 1, 1));
		btTransform transform = btTransform(rotation, position);
		body = physics.createRigid(1, transform, shape, COL_OBJECT, COL_EVERYTHING);
	}

	void render() {
		btTransform transform = body->getWorldTransform();
		float m[16];
		transform.getOpenGLMatrix(m);
		glm::mat4 model = glm::make_mat4(m);
		program.setMat4("model", model);
		program.set4f("frag_Color", glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));

		box.render(program);
	}

	void release() {
		physics.removeRigidBody(body);
		delete shape;
		box.release();
	}

};

struct SphereObject {
	GeometrySphere box;
	btRigidBody* body;
	btCollisionShape* shape;

	void init(const btQuaternion& rotation, const btVector3& position) {
		box.init();
		shape = physics.createBoxShape(btVector3(1, 1, 1));
		btTransform transform = btTransform(rotation, position);
		body = physics.createRigid(1, transform, shape, COL_OBJECT, COL_EVERYTHING);
	}

	void render() {
		btTransform transform = body->getWorldTransform();
		float m[16];
		transform.getOpenGLMatrix(m);
		glm::mat4 model = glm::make_mat4(m);
		program.setMat4("model", model);
		program.set4f("frag_Color", glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));

		box.render(program);
	}

	void release() {
		physics.removeRigidBody(body);
		delete shape;
		box.release();
	}

};


enum PhysicsOptions {
	PO_PUSH = 0,
	PO_PULL,
	PO_MASS_PUSH,
	PO_MASS_PULL,
	PO_GRAB_BODY
};

struct PhysicsCamera {
	btRigidBody* body = nullptr;
	btCollisionShape* shape = nullptr;
	glm::vec2 rot;

	float fov;
	float aspect;
	float znear;
	float zfar;

	float speed = 64.0f;
	float walkSpeed = 512.0f;
	float jumpSpeed = 512.0f;

	PhysicsOptions options = PhysicsOptions::PO_PUSH;

	btRigidBody* grabbed = nullptr;

	void init(
		btVector3 position,
		glm::vec2 rotation,
		float fov,
		float aspect,
		float znear,
		float zfar) {

		this->shape = physics.createCapsuleShape(1.0f, 2.0f);

		btTransform transform = btTransform(btQuaternion(0, 0, 0, 1), position);

		this->body = physics.createRigid(1.0f, transform, this->shape, COL_CAMERA, COL_EVERYTHING);
		this->body->setSleepingThresholds(0.0f, 0.0f);
		this->body->setAngularFactor(0.0f);

		this->rot = rotation;

		this->fov = fov;
		this->aspect = aspect;
		this->znear = znear;
		this->zfar = zfar;

		SDL_SetRelativeMouseMode(SDL_TRUE);

	}

	void doEvent(SDL_Event& e) {

		if (e.type == SDL_KEYUP) {
			switch (e.key.keysym.scancode) {
			case SDL_SCANCODE_1:
				options = PhysicsOptions::PO_PUSH;
				grabbed = nullptr;
				std::cout << "PhysicsOptions: RAY PUSH MODE." << std::endl;
				break;
			case SDL_SCANCODE_2:
				options = PhysicsOptions::PO_PULL;
				grabbed = nullptr;
				std::cout << "PhysicsOptions: RAY PULL MODE." << std::endl;
				break;
			case SDL_SCANCODE_3:
				options = PhysicsOptions::PO_MASS_PUSH;
				grabbed = nullptr;
				std::cout << "PhysicsOptions: MASS PUSH MODE." << std::endl;
				break;
			case SDL_SCANCODE_4:
				options = PhysicsOptions::PO_MASS_PULL;
				grabbed = nullptr;
				std::cout << "PhysicsOptions: MASS PULL MODE." << std::endl;
				break;
			case SDL_SCANCODE_5:
				options = PhysicsOptions::PO_GRAB_BODY;
				std::cout << "PhysicsOptions: GRAB BODY MODE." << std::endl;
				break;
			default:
				break;
			}
		}

		std::function<void(float)> phyRayPush = [&](float force) {
			btVector3 rayTo = this->pickRay(g_width / 2, g_height / 2);
			btVector3 pos = this->body->getCenterOfMassPosition();
			pos.setY(pos.y() + 1.0f);
			btCollisionWorld::ClosestRayResultCallback rayCallback(pos, rayTo);
			rayCallback.m_collisionFilterGroup = COL_OBJECT;
			rayCallback.m_collisionFilterMask = COL_OBJECT;
			physics.dynamicWorld->rayTest(pos, rayTo, rayCallback);
			debugLine.setLine(pos, rayTo);
			if (rayCallback.hasHit()) {
				debugLine.setLine(pos, rayCallback.m_hitPointWorld);
				btRigidBody* b = (btRigidBody*)btRigidBody::upcast(rayCallback.m_collisionObject);
				b->activate(true);
				// Used for Push
				btVector3 dir = b->getCenterOfMassPosition() - pos;
				dir.normalize();
				dir *= force;
				b->setLinearVelocity(dir);
			}
		};

		std::function<void(float)> phyRayPull = [&](float force) {
			btVector3 rayTo = this->pickRay(g_width / 2, g_height / 2);
			btVector3 pos = this->body->getCenterOfMassPosition();
			pos.setY(pos.y() + 1.0f);
			btCollisionWorld::ClosestRayResultCallback rayCallback(pos, rayTo);
			rayCallback.m_collisionFilterGroup = COL_OBJECT;
			rayCallback.m_collisionFilterMask = COL_OBJECT;
			physics.dynamicWorld->rayTest(pos, rayTo, rayCallback);
			debugLine.setLine(pos, rayTo);
			if (rayCallback.hasHit()) {
				debugLine.setLine(pos, rayCallback.m_hitPointWorld);
				btRigidBody* b = (btRigidBody*)btRigidBody::upcast(rayCallback.m_collisionObject);
				b->activate(true);
				// Used for Push
				btVector3 dir = pos - b->getCenterOfMassPosition();
				dir.normalize();
				dir *= force;
				b->setLinearVelocity(dir);
			}
		};

		std::function<void(const btVector3&, float)> phyMassPush = [&](const btVector3& offsets, float force) {
			std::vector<btRigidBody*> bodies;
			btVector3 point = body->getCenterOfMassPosition();
			btVector3 minAABB = point - offsets;
			btVector3 maxAABB = offsets + point;
			physics.getRigidBodiesFromAABB(minAABB, maxAABB, bodies, COL_OBJECT);

			std::cout << bodies.size() << std::endl;

			for (int i = 0; i < bodies.size(); i++) {
				btVector3 other = bodies[i]->getCenterOfMassPosition();
				btVector3 dir = other - point;
				dir.normalize();
				dir *= force;
				bodies[i]->activate(true);
				bodies[i]->setLinearVelocity(dir);
			}
		};

		std::function<void(const btVector3&, float)> phyMassPull = [&](const btVector3& offsets, float force) {
			std::vector<btRigidBody*> bodies;
			btVector3 point = body->getCenterOfMassPosition();
			btVector3 minAABB = point - offsets;
			btVector3 maxAABB = offsets + point;
			physics.getRigidBodiesFromAABB(minAABB, maxAABB, bodies, COL_OBJECT);
			for (int i = 0; i < bodies.size(); i++) {
				btVector3 other = bodies[i]->getCenterOfMassPosition();
				btVector3 dir = point - other;
				dir.normalize();
				dir *= force;
				bodies[i]->activate(true);
				bodies[i]->setLinearVelocity(dir);
			}
		};

		std::function<void()> grabRigidBody = [&]() {
			btVector3 rayTo = this->pickRay(g_width / 2, g_height / 2);
			btVector3 pos = this->body->getCenterOfMassPosition();
			pos.setY(pos.y() + 1.0f);
			btCollisionWorld::ClosestRayResultCallback rayCallback(pos, rayTo);
			rayCallback.m_collisionFilterGroup = COL_OBJECT;
			rayCallback.m_collisionFilterMask = COL_OBJECT;
			physics.dynamicWorld->rayTest(pos, rayTo, rayCallback);
			debugLine.setLine(pos, rayTo);
			if (rayCallback.hasHit()) {
				debugLine.setLine(pos, rayTo);
				this->grabbed = (btRigidBody*)btRigidBody::upcast(rayCallback.m_collisionObject);
			}
		};

		if (e.type == SDL_MOUSEBUTTONUP) {
			if (e.button.button == SDL_BUTTON_LEFT) {

				if (this->grabbed == nullptr) {
					switch (this->options) {
					case PhysicsOptions::PO_PUSH:
						phyRayPush(64);
						break;
					case PhysicsOptions::PO_PULL:
						phyRayPull(64);
						break;
					case PhysicsOptions::PO_MASS_PUSH:
						phyMassPush(btVector3(32, 32, 32), 64);
						break;
					case PhysicsOptions::PO_MASS_PULL:
						phyMassPull(btVector3(32, 32, 32), 64);
						break;
					case PhysicsOptions::PO_GRAB_BODY:
						grabRigidBody();
						break;
					}
				}
				else {
					grabbed = nullptr;
				}
			}
			else if (e.button.button == SDL_BUTTON_RIGHT) {

				if (this->grabbed != nullptr) {

					if (this->options == PhysicsOptions::PO_GRAB_BODY) {
						btVector3 rayTo = this->pickRay(g_width / 2, g_height / 2);

						btVector3 dir = rayTo - body->getCenterOfMassPosition();

						dir.normalize();

						dir *= 128.0f;

						grabbed->activate(true);
						grabbed->setLinearVelocity(dir);
						grabbed = nullptr;
					}
				}
			}
		}
	}

	void update(float delta) {
		int x = 0, y = 0;
		uint32_t buttons = SDL_GetRelativeMouseState(&x, &y);
		const uint8_t* keys = SDL_GetKeyboardState(nullptr);

		body->activate(true);

		rot.x += this->speed * y * ((delta - 0.001f < 0) ? 0.001f : delta);
		rot.y += this->speed * x * ((delta - 0.001f < 0) ? 0.001f : delta);

		if (rot.y <= -360.0f) {
			rot.y += 360.0f;
		}

		if (rot.y >= 360.0f) {
			rot.y -= 360.0f;
		}

		if (rot.x < -90.0f) {
			rot.x = -90.0f;
		}

		if (rot.x > 90.0f) {
			rot.x = 90.0f;
		}

		float yrad = glm::radians(rot.y);

		float sp = this->walkSpeed;

		if (keys[SDL_SCANCODE_E]) {
			sp *= 3.0f;
		}

		btVector3 vel = body->getLinearVelocity();
		
		vel[0] = 0;
		vel[2] = 0;

		if (keys[SDL_SCANCODE_SPACE]) {
			vel[1] = this->jumpSpeed * FIXED_FRAME_60;
		}

		if (keys[SDL_SCANCODE_W]) {
			vel[0] += sp * btSin(yrad) * FIXED_FRAME_60;
			vel[2] -= sp * btCos(yrad) * FIXED_FRAME_60;
		}

		if (keys[SDL_SCANCODE_S]) {
			vel[0] -= sp * btSin(yrad) * FIXED_FRAME_60;
			vel[2] += sp * btCos(yrad) * FIXED_FRAME_60;
		}

		if (keys[SDL_SCANCODE_A]) {
			vel[0] -= sp * btCos(yrad) * FIXED_FRAME_60;
			vel[2] -= sp * btSin(yrad) * FIXED_FRAME_60;
		}

		if (keys[SDL_SCANCODE_D]) {
			vel[0] += sp * btCos(yrad) * FIXED_FRAME_60;
			vel[2] += sp * btSin(yrad) * FIXED_FRAME_60;
		}

		body->setLinearVelocity(vel);
	}

	void fixedUpdate() {
		if (this->options == PhysicsOptions::PO_GRAB_BODY && this->grabbed != nullptr) {
			glm::mat4 trans = glm::translate(glm::mat4(1.0f), glm::vec3(.5f, .5f, 5.0f));

			glm::mat4 movement = trans * this->getView();

			movement = glm::inverse(movement);

			glm::vec3 v = glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f));

			glm::vec4 v4 = glm::vec4(v, 1.0f);

			v4 = movement * v4;

			btTransform tran = grabbed->getCenterOfMassTransform();

			tran.setOrigin(btVector3(v4.x, v4.y, v4.z));
			tran.setRotation(btQuaternion(btRadians(-rot.y), btRadians(-rot.x), 0.0f));
			grabbed->activate(true);
			grabbed->setCenterOfMassTransform(tran);
		}
	}

	glm::mat4 getProjection() {
		return glm::perspective(
			glm::radians(this->fov),
			aspect,
			znear,
			zfar);
	}

	glm::mat4 getView() {
		btVector3 position = body->getCenterOfMassPosition();

		glm::vec3 pos = glm::vec3(
			position.x(),
			position.y() + 1.0f,
			position.z()
		);

		return	
				glm::rotate(glm::mat4(1.0f), glm::radians(this->rot.x), glm::vec3(1.0f, 0.0f, 0.0f)) *
				glm::rotate(glm::mat4(1.0f), glm::radians(this->rot.y), glm::vec3(0.0f, 1.0f, 0.0f)) *
				glm::translate(glm::mat4(1.0f), -pos);
	}

	btVector3 pickRay(int x, int y) {
		glm::vec3 coords;

		coords.x = (((2.0f * x) / g_width) - 1);
		coords.y = -(((2.0f * y) / g_height) - 1);
		coords.z = -1.0f;

		glm::mat4 proj = this->getProjection();
		glm::mat4 view = this->getView();

		coords.x /= proj[1][1];
		coords.y /= proj[2][2];

		coords *= this->zfar;

		glm::mat4 invView = glm::inverse(view);

		coords = glm::mat3(invView) * coords;

		return btVector3(coords.x, coords.y, coords.z);
	}

	void release() {
		physics.removeRigidBody(this->body);
		delete this->shape;
	}

};

float rotY = 0.0f;

PhysicsCamera camera;

FloorObject floorObject;
//BoxObject boxObject;

std::vector<BoxObject> boxObjects;
std::vector<SphereObject> sphereObjects;


Texture2D crosshairTex;
GeometryQuad crosshairQuad;
bool isDebugLine = false;
//bool isWireFrame = false;

enum PolyMode {
	PM_FILL = 0,
	PM_LINE,
	PM_POINT
};

PolyMode polyMode = PolyMode::PM_FILL;

void reset_Objects() {
	// Boxes
	for (uint32_t i = 0; i < 32; i++) {
		int x = (rand() % 40) - 20;
		int y = (rand() % 160) + 32;
		int z = (rand() % 40) - 20;

		int rx = rand() % 361;
		int ry = rand() % 361;
		int rz = rand() % 361;

		btTransform transform = btTransform(btQuaternion(rx, ry, rz), btVector3(x, y, z));
		
		boxObjects[i].body->setAngularVelocity(btVector3(0, 0, 0));
		boxObjects[i].body->setLinearVelocity(btVector3(0, 0, 0));
		boxObjects[i].body->setWorldTransform(transform);
		boxObjects[i].body->activate(true);
	}
	// Spheres
	for (uint32_t i = 0; i < 32; i++) {
		int x = (rand() % 40) - 20;
		int y = (rand() % 160) + 32;
		int z = (rand() % 40) - 20;

		btTransform transform = btTransform(btQuaternion(0, 0, 0, 1), btVector3(x, y, z));

		sphereObjects[i].body->setAngularVelocity(btVector3(0, 0, 0));
		sphereObjects[i].body->setLinearVelocity(btVector3(0, 0, 0));
		sphereObjects[i].body->setWorldTransform(transform);
		sphereObjects[i].body->activate(true);
	}
}

void app_init() {

	srand(time(nullptr));

	glEnable(GL_DEPTH_TEST);

	vertexShader.init(GL_VERTEX_SHADER, "data/shaders/main.vs.glsl");
	fragmentShader.init(GL_FRAGMENT_SHADER, "data/shaders/main.fs.glsl");

	program.addShader(&vertexShader);
	program.addShader(&fragmentShader);

	program.init();

	program.bind();

	// Create Uniforms
	program.createUniform("proj");
	program.createUniform("view");
	program.createUniform("model");
	program.createUniform("frag_Color");
	program.set4f("frag_Color", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

	// Create Attributes
	program.setAttribute("vertices", 0);

	program.bindAttribute();
	program.enableAttribute("vertices");
	program.unbindAttribute();
	program.disableAttribute("vertices");

	program.unbind();


	// HUB Shaders
	hubVertexShader.init(GL_VERTEX_SHADER, "data/shaders/hub.vs.glsl");
	hubFragmentShader.init(GL_FRAGMENT_SHADER, "data/shaders/hub.fs.glsl");

	hubProgram.addShader(&hubVertexShader);
	hubProgram.addShader(&hubFragmentShader);

	hubProgram.init();

	hubProgram.bind();
	hubProgram.createUniform("proj");
	hubProgram.createUniform("view");
	hubProgram.createUniform("model");
	hubProgram.createUniform("tex0");
	hubProgram.set1i("tex0", 0);

	hubProgram.setAttribute("vertices", 0);
	hubProgram.setAttribute("texCoords", 1);

	hubProgram.bindAttribute();
	hubProgram.enableAttribute("vertices");
	hubProgram.enableAttribute("texCoords");
	hubProgram.unbindAttribute();
	hubProgram.disableAttribute("vertices");
	hubProgram.disableAttribute("texCoords");

	hubProgram.unbind();

	physics.init();

	camera.init(
		btVector3(0.0f, 2.0f, 0.0f),
		glm::vec2(0.0f, 0.0f),
		60.0f,
		(float)g_width / (float)g_height,
		1.0f,
		1024.0f);


	floorObject.init();

	for (uint32_t i = 0; i < 32; i++) {
		int x = (rand() % 40) - 20;
		int y = (rand() % 160) + 32;
		int z = (rand() % 40) - 20;

		int rx = rand() % 361;
		int ry = rand() % 361;
		int rz = rand() % 361;

		BoxObject temp;

		temp.init(btQuaternion(btRadians(rx), btRadians(ry), btRadians(rz)), btVector3(x, y, z));

		boxObjects.push_back(temp);
	}

	for (uint32_t i = 0; i < 32; i++) {
		int x = (rand() % 40) - 20;
		int y = (rand() % 160) + 32;
		int z = (rand() % 40) - 20;

		SphereObject temp;
		temp.init(btQuaternion(0, 0, 0, 1), btVector3(x, y, z));
		sphereObjects.push_back(temp);
	}

	crosshairTex.init("data/textures/crosshair.png");
	crosshairQuad.init();
	
	debugLine.init();
}

void app_event(SDL_Event& e) {
	if (e.type == SDL_KEYUP) {
		if (e.key.keysym.scancode == SDL_SCANCODE_Q) {
			reset_Objects();
		}

		if (e.key.keysym.scancode == SDL_SCANCODE_F1) {
			isDebugLine = !isDebugLine;
		}

		if (e.key.keysym.scancode == SDL_SCANCODE_F2) {
			if (polyMode == PolyMode::PM_FILL) {
				polyMode = PolyMode::PM_LINE;
			}
			else if (polyMode == PolyMode::PM_LINE) {
				polyMode = PolyMode::PM_POINT;
			}
			else if (polyMode == PolyMode::PM_POINT) {
				polyMode = PolyMode::PM_FILL;
			}

			switch (polyMode) {
			case PolyMode::PM_FILL:
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
				break;
			case PolyMode::PM_LINE:
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
				break;
			case PolyMode::PM_POINT:
				glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
				break;
			}
		}
	}

	camera.doEvent(e);
}

void app_update(float delta) {

	const uint8_t* keys = SDL_GetKeyboardState(nullptr);

	if (keys[SDL_SCANCODE_ESCAPE]) {
		g_running = false;
	}

	camera.update(delta);
}

void app_fixedUpdate() {
	physics.stepSimulation();
	camera.fixedUpdate();
}

void app_render() {
	glClear(
		GL_COLOR_BUFFER_BIT | 
		GL_DEPTH_BUFFER_BIT);

	// Render 3D
	program.bind();

	program.setMat4("proj", camera.getProjection());
	program.setMat4("view", camera.getView());

	floorObject.render();
	//boxObject.render();

	for (int i = 0; i < boxObjects.size(); i++) {
		boxObjects[i].render();
	}

	for (int i = 0; i < sphereObjects.size(); i++) {
		sphereObjects[i].render();
	}

	if (isDebugLine) {
		debugLine.render(program);
	}

	program.unbind();

	glm::mat4 proj = glm::ortho(0.0f, (float)g_width, (float)g_height, 0.0f);
	glm::mat4 view = glm::mat4(1.0f);
	glm::mat4 model =
		glm::translate(glm::mat4(1.0f), glm::vec3(g_width * 0.5f, g_height * 0.5f, 0.0f)) *
		glm::scale(glm::mat4(1.0f), glm::vec3(16.0f, 16.0f, 0.0f));


	// Render HUB

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);

	hubProgram.bind();

	hubProgram.setMat4("proj", proj);
	hubProgram.setMat4("view", view);
	hubProgram.setMat4("model", model);

	crosshairTex.bind(GL_TEXTURE0);

	crosshairQuad.render(hubProgram);

	crosshairTex.unbind(GL_TEXTURE0);
	
	hubProgram.unbind();

	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
}

void app_release() {
	debugLine.release();

	crosshairQuad.release();
	crosshairTex.release();

	for (int i = 0; i < sphereObjects.size(); i++) {
		sphereObjects[i].release();
	}
	sphereObjects.clear();

	for (int i = 0; i < boxObjects.size(); i++) {
		boxObjects[i].release();
	}
	boxObjects.clear();
	floorObject.release();
	camera.release();

	physics.release();


	hubProgram.release();
	hubFragmentShader.release();
	hubVertexShader.release();

	program.release();

	fragmentShader.release();
	vertexShader.release();
}

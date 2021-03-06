#include "pch.h"

#include <Kore/Application.h>
#include <Kore/IO/FileReader.h>
#include <Kore/Math/Core.h>
#include <Kore/Math/Random.h>
#include <Kore/System.h>
#include <Kore/Input/Keyboard.h>
#include <Kore/Input/KeyEvent.h>
#include <Kore/Input/Mouse.h>
#include <Kore/Audio/Mixer.h>
#include <Kore/Graphics/Image.h>
#include <Kore/Graphics/Graphics.h>
#include <Kore/Log.h>
#include "ObjLoader.h"

#include "Collision.h"
#include "PhysicsWorld.h"
#include "PhysicsObject.h"

using namespace Kore;

// A simple particle implementation
class Particle {
public:
	VertexBuffer* vb;
	IndexBuffer* ib;

	mat4 M;
	
	// The current position
	vec3 position;
	
	// The current velocity
	vec3 velocity;

	// The remaining time to live
	float timeToLive;

	// The total time time to live
	float totalTimeToLive;

	// Is the particle dead (= ready to be re-spawned?)
	bool dead;



	void init(const VertexStructure& structure) {
		vb = new VertexBuffer(4, structure);
		float* vertices = vb->lock();
		SetVertex(vertices, 0, -1, -1, 0, 0, 0);
		SetVertex(vertices, 1, -1, 1, 0, 0, 1);
		SetVertex(vertices, 2, 1, 1, 0, 1, 1); 
		SetVertex(vertices, 3, 1, -1, 0, 1, 0); 
		vb->unlock();

		// Set index buffer
		ib = new IndexBuffer(6);
		int* indices = ib->lock();
		indices[0] = 0;
		indices[1] = 1;
		indices[2] = 2;
		indices[3] = 0;
		indices[4] = 2;
		indices[5] = 3;
		ib->unlock();

		dead = true;
	}


	void Emit(vec3 pos, vec3 velocity, float timeToLive) {
		position = pos;
		this->velocity = velocity;
		dead = false;
		this->timeToLive = timeToLive;
		totalTimeToLive = timeToLive;
	}

	Particle() {
	}


	void SetVertex(float* vertices, int index, float x, float y, float z, float u, float v) {
		vertices[index* 8 + 0] = x;
		vertices[index*8 + 1] = y;
		vertices[index*8 + 2] = z;
		vertices[index*8 + 3] = u;
		vertices[index*8 + 4] = v;
		vertices[index*8 + 5] = 0.0f;
		vertices[index*8 + 6] = 0.0f;
		vertices[index*8 + 7] = -1.0f;
	}

	void render(TextureUnit tex, Texture* image) {
		image->set(tex);
		vb->set();
		ib->set();
		Graphics::drawIndexedVertices();
	}

	void Integrate(float deltaTime) {
		timeToLive -= deltaTime;

		if (timeToLive < 0.0f) {
			dead = true;
		}
		
		// Note: We are using no forces or gravity at the moment.

		position += velocity * deltaTime;

		// Build the matrix
		M = mat4::Translation(position.x(), position.y(), position.z()) * mat4::Scale(0.2f, 0.2f, 0.2f);
	}


};


class ParticleSystem {
public:

	// The center of the particle system
	vec3 position;

	// The minimum coordinates of the emitter box
	vec3 emitMin;

	// The maximal coordinates of the emitter box
	vec3 emitMax;
	
	// The list of particles
	Particle* particles;

	// The number of particles
	int numParticles;

	// The spawn rate
	float spawnRate;
	
	// When should the next particle be spawned?
	float nextSpawn;

	ParticleSystem(int maxParticles, const VertexStructure& structure ) {
		particles = new Particle[maxParticles];
		numParticles = maxParticles;
		for (int i = 0; i < maxParticles; i++) {
			particles[i].init(structure);
		}
		spawnRate = 0.05f;
		nextSpawn = spawnRate;

		position = vec3(0.5f, 1.3f, 0.5f);
		float b = 0.1f;
		emitMin = position + vec3(-b, -b, -b);
		emitMax = position + vec3(b, b, b);
	}

	
	void update(float deltaTime) {
		// Do we need to spawn a particle?
		nextSpawn -= deltaTime;
		bool spawnParticle = false;
		if (nextSpawn < 0) {
			spawnParticle = true;
			nextSpawn = spawnRate;
		}
		
		
		for (int i = 0; i < numParticles; i++) {
			
			if (particles[i].dead) {
				if (spawnParticle) {
					EmitParticle(i);
					spawnParticle = false;
				}
			}

			particles[i].Integrate(deltaTime);
		}
	}

	void render(TextureUnit tex, Texture* image, ConstantLocation mLocation, mat4 V) {
		Graphics::setBlendingMode(SourceAlpha, InverseSourceAlpha);
		Graphics::setRenderState(DepthWrite, false);
		
		
		// Calculate the matrix to multiply the billboards with to align them to the view
		// Task 1.1: Carry out the correct calculations here
		mat4 billboard = mat4::Identity();
		

		for (int i = 0; i < numParticles; i++) {
			// Skip dead particles
			if (particles[i].dead) continue;


			Graphics::setMatrix(mLocation, particles[i].M * V);


			particles[i].render(tex, image);
		}
		Graphics::setRenderState(DepthWrite, true);
	}

	float getRandom(float minValue, float maxValue) {
		int randMax = 1000000;
		int randInt = Random::get(0, randMax);
		float r =  (float) randInt / (float) randMax;
		return minValue + r * (maxValue - minValue);
	}

	void EmitParticle(int index) {
		// Calculate a random position inside the box
		float x = getRandom(emitMin.x(), emitMax.x());
		float y = getRandom(emitMin.y(), emitMax.y());
		float z = getRandom(emitMin.z(), emitMax.z());

		vec3 pos;
		pos.set(x, y, z);

		vec3 velocity(0, 0.3f, 0);

		particles[index].Emit(pos, velocity, 3.0f);
	}


};
























namespace {
	const int width = 1024;
	const int height = 768;
	double startTime;
	Shader* vertexShader;
	Shader* fragmentShader;
	Program* program;

	float angle = 0.0f;

	// null terminated array of MeshObject pointers
	MeshObject* objects[] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

	// null terminated array of PhysicsObject pointers
	PhysicsObject* physicsObjects[] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };


	// The view projection matrix aka the camera
	mat4 P;
	mat4 View;
	mat4 PV;

	vec3 cameraPosition;

	MeshObject* sphere;
	PhysicsObject* po;

	PhysicsWorld physics;
	

	// uniform locations - add more as you see fit
	TextureUnit tex;
	ConstantLocation pvLocation;
	ConstantLocation mLocation;
	ConstantLocation tintLocation;

	Texture* particleImage;
	ParticleSystem* particleSystem;

	double lastTime;

	void update() {
		double t = System::time() - startTime;
		double deltaT = t - lastTime;
		//Kore::log(Info, "%f\n", deltaT);
		lastTime = t;
		Kore::Audio::update();
		
		Graphics::begin();
		Graphics::clear(Graphics::ClearColorFlag | Graphics::ClearDepthFlag, 0xff9999FF, 1000.0f);

		Graphics::setFloat4(tintLocation, vec4(1, 1, 1, 1));

		program->set();

		// set the camera

		angle += 0.3f * deltaT;

		float x = 0 + 3 * Kore::cos(angle);
		float z = 0 + 3 * Kore::sin(angle);
		
		cameraPosition.set(x, 2, z);

		P = mat4::Perspective(60, (float)width / (float)height, 0.1f, 100);
		View = mat4::lookAt(vec3(x, 2, z), vec3(0, 2, 0), vec3(0, 1, 0));
		PV = P * View;


		Graphics::setMatrix(pvLocation, PV);





		// iterate the MeshObjects
		MeshObject** current = &objects[0];
		while (*current != nullptr) {
			// set the model matrix
			Graphics::setMatrix(mLocation, (*current)->M);

			(*current)->render(tex);
			++current;
		} 

		

		// Update the physics
		physics.Update(deltaT);
		// physics.HandleCollisions(deltaT);

		Graphics::setFloat4(tintLocation, vec4(1, 1, 1, 1));

		PhysicsObject** currentP = &physics.physicsObjects[0];
		while (*currentP != nullptr) {
			(*currentP)->UpdateMatrix();
			Graphics::setMatrix(mLocation, (*currentP)->Mesh->M);
			(*currentP)->Mesh->render(tex);
			++currentP;
		}
		


		particleSystem->update(deltaT);
		particleSystem->render(tex, particleImage, mLocation, View);



		Graphics::end();
		Graphics::swapBuffers();
	}

	void SpawnSphere(vec3 Position, vec3 Velocity) {
		PhysicsObject* po = new PhysicsObject();
		po->SetPosition(Position);
		po->Velocity = Velocity;
		po->Collider.radius = 0.2f;

		po->Mass = 5;
		po->Mesh = sphere;
			
		// The impulse should carry the object forward
		// Use the inverse of the view matrix

		po->ApplyImpulse(Velocity);
		physics.AddObject(po);
	}

	void keyDown(KeyEvent* event) {
		if (event->keycode() == Key_Space) {
			
			// The impulse should carry the object forward
			// Use the inverse of the view matrix

			vec4 impulse(0, 0.4, 2, 0);
			mat4 viewI = View;
			viewI.Invert();
			impulse = viewI * impulse;
			
			vec3 impulse3(impulse.x(), impulse.y(), impulse.z());

			
			SpawnSphere(cameraPosition + impulse3 *0.2f, impulse3);
		}
	}

	void keyUp(KeyEvent* event) {
		if (event->keycode() == Key_Left) {
			// ...
		}
	}

	void mouseMove(int x, int y) {

	}
	
	void mousePress(int button, int x, int y) {

	}

	

	void mouseRelease(int button, int x, int y) {
		
	}

	void init() {
		FileReader vs("shader.vert");
		FileReader fs("shader.frag");
		vertexShader = new Shader(vs.readAll(), vs.size(), VertexShader);
		fragmentShader = new Shader(fs.readAll(), fs.size(), FragmentShader);

		// This defines the structure of your Vertex Buffer
		VertexStructure structure;
		structure.add("pos", Float3VertexData);
		structure.add("tex", Float2VertexData);
		structure.add("nor", Float3VertexData);

		program = new Program;
		program->setVertexShader(vertexShader);
		program->setFragmentShader(fragmentShader);
		program->link(structure);
		program->set();

		tex = program->getTextureUnit("tex");
		pvLocation = program->getConstantLocation("PV");
		mLocation = program->getConstantLocation("M");
		tintLocation = program->getConstantLocation("tint");

		objects[0] = new MeshObject("Base.obj", "Level/basicTiles6x6.png", structure);
		objects[0]->M = mat4::Translation(0.0f, 1.0f, 0.0f);

		sphere = new MeshObject("ball_at_origin.obj", "Level/unshaded.png", structure);

		SpawnSphere(vec3(0, 2, 0), vec3(0, 0, 0));
		
		

		Graphics::setRenderState(DepthTest, true);
		Graphics::setRenderState(DepthTestCompare, ZCompareLess);

		Graphics::setTextureAddressing(tex, U, Repeat);
		Graphics::setTextureAddressing(tex, V, Repeat);

		particleImage = new Texture("SuperParticle.png", true);
		particleSystem = new ParticleSystem(100, structure);

		

	}
}

int kore(int argc, char** argv) {
	Application* app = new Application(argc, argv, width, height, false, "Exercise8");
	
	init();

	app->setCallback(update);

	startTime = System::time();
	lastTime = 0.0f;
	Kore::Mixer::init();
	Kore::Audio::init();
	
	
	Keyboard::the()->KeyDown = keyDown;
	Keyboard::the()->KeyUp = keyUp;
	Mouse::the()->Move = mouseMove;
	Mouse::the()->Press = mousePress;
	Mouse::the()->Release = mouseRelease;

	app->start();

	delete app;
	
	return 0;
}

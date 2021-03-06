#include "pch.h"
#include "../ogre/Defines.h"
#include "collision_world.h"

#include "tobullet.h"
#include "collision_contact.h"
#include "model.h"
#include "track.h"
#include "cardynamics.h"
//#include "car.h"//

#include <OgreLogManager.h>

///  ctor bullet world
///----------------------------------------------------------------------------
COLLISION_WORLD::COLLISION_WORLD() :
	config(0), dispatcher(0), broadphase(0), solver(0), world(0),
	track(NULL), trackObject(NULL), trackMesh(NULL),
	fixedTimestep(1.f/60.f), maxSubsteps(7)  // default, set from settings
{
	config = new btDefaultCollisionConfiguration();
	dispatcher = new btCollisionDispatcher(config);

	broadphase = new bt32BitAxisSweep3(btVector3(-5000, -5000, -5000), btVector3(5000, 5000, 5000));
	solver = new btSequentialImpulseConstraintSolver();
	world = new DynamicsWorld(dispatcher, broadphase, solver, config);

	world->setGravity(btVector3(0.0, 0.0, -9.81)); ///~
	//world->getSolverInfo().m_numIterations = 36;  //-
	//world->getSolverInfo().m_erp = 0.3;
	//world->getSolverInfo().m_erp2 = 0.2;
	world->getSolverInfo().m_restitution = 0.0f;
	world->getDispatchInfo().m_enableSPU = true;
	world->setForceUpdateAllAabbs(false);  //+
}

COLLISION_WORLD::~COLLISION_WORLD()
{
	Clear();

	delete world;
	delete solver;
	delete broadphase;
	delete dispatcher;
	delete config;
}

void COLLISION_WORLD::DebugDrawScene()
{
}

btCollisionObject * COLLISION_WORLD::AddCollisionObject(const MODEL & model)
{
	btCollisionObject * col = new btCollisionObject();
	btCollisionShape * shape = AddMeshShape(model);
	col->setCollisionShape(shape);
	world->addCollisionObject(col);
	return col;
}

btRigidBody * COLLISION_WORLD::AddRigidBody(const btRigidBody::btRigidBodyConstructionInfo & info,
	bool car, bool bCarsCollis)
{
	btRigidBody * body = new btRigidBody(info);
	btCollisionShape * shape = body->getCollisionShape();
	//body->setActivationState(DISABLE_DEACTIVATION);  //!-for chassis only
	//body->setCollisionFlags
	#define  COL_CAR  (1<<2)
	if (car)  world->addRigidBody(body, COL_CAR, 255 - (!bCarsCollis ? COL_CAR : 0));  // group, mask
	else	  world->addRigidBody(body);
	shapes.push_back(shape);
	return body;
}

void COLLISION_WORLD::AddAction(btActionInterface * action)
{
	world->addAction(action);
	actions.push_back(action);
}

void COLLISION_WORLD::AddConstraint(btTypedConstraint * constraint, bool disableCollisionsBetweenLinked)
{
	world->addConstraint(constraint, disableCollisionsBetweenLinked);
	constraints.push_back(constraint);
}

void COLLISION_WORLD::SetTrack(TRACK * t)
{
	assert(t);
	
	// remove old track
	if(track)
	{
		world->removeCollisionObject(trackObject);
		
		delete trackObject->getCollisionShape();
		trackObject->setCollisionShape(NULL);
		
		delete trackObject;
		trackObject = NULL;
		
		delete trackMesh;
		trackMesh = NULL;
	}
	
	// setup new track
	track = t;
	trackMesh = new btTriangleIndexVertexArray();
	trackSurface.resize(0);
	const std::list<TRACK_OBJECT> & objects = track->GetTrackObjects();
	for(std::list<TRACK_OBJECT>::const_iterator ob = objects.begin(); ob != objects.end(); ob++)
	{
		if(ob->GetSurface() != NULL)
		{
			MODEL & model = *ob->GetModel();
			btIndexedMesh mesh = GetIndexedMesh(model);
			trackMesh->addIndexedMesh(mesh);
			const TRACKSURFACE * surface = ob->GetSurface();
			trackSurface.push_back(surface);
		}
	}
	
	//  no objs track
	if (trackSurface.size()==0)
	{
		static TRACKSURFACE surface;
		trackSurface.push_back(&surface);
	}
	else  ///
	{
	// can not use QuantizedAabbCompression because of the track size
	btCollisionShape * trackShape = new btBvhTriangleMeshShape(trackMesh, false);
	trackObject = new btCollisionObject();
	trackObject->setCollisionShape(trackShape);
	trackObject->setUserPointer(NULL);
	
	world->addCollisionObject(trackObject);
	}
}

btIndexedMesh COLLISION_WORLD::GetIndexedMesh(const MODEL & model)
{
	const float * vertices;  int vcount;
	const int * faces;  int fcount;
	model.GetVertexArray().GetVertices(vertices, vcount);
	model.GetVertexArray().GetFaces(faces, fcount);
	
	assert(fcount % 3 == 0); //Face count is not a multiple of 3
	
	btIndexedMesh mesh;
	mesh.m_numTriangles = fcount / 3;
	mesh.m_triangleIndexBase = (const unsigned char *)faces;
	mesh.m_triangleIndexStride = sizeof(int) * 3;
	mesh.m_numVertices = vcount;
	mesh.m_vertexBase = (const unsigned char *)vertices;
	mesh.m_vertexStride = sizeof(float) * 3;
	mesh.m_vertexType = PHY_FLOAT;
	return mesh;
}

btCollisionShape * COLLISION_WORLD::AddMeshShape(const MODEL & model)
{
	btTriangleIndexVertexArray * mesh = new btTriangleIndexVertexArray();
	mesh->addIndexedMesh(GetIndexedMesh(model));
	btCollisionShape * shape = new btBvhTriangleMeshShape(mesh, true);
	
	meshes.push_back(mesh);
	shapes.push_back(shape);
	
	return shape;
}


struct MyRayResultCallback : public btCollisionWorld::RayResultCallback
{
	MyRayResultCallback(const btVector3 & rayFromWorld, const btVector3 & rayToWorld, const btCollisionObject * exclude)
	:m_rayFromWorld(rayFromWorld), m_rayToWorld(rayToWorld), m_exclude(exclude)
	{	}

	btVector3	m_rayFromWorld;//used to calculate hitPointWorld from hitFraction
	btVector3	m_rayToWorld;

	btVector3	m_hitNormalWorld;
	btVector3	m_hitPointWorld;
	
	int m_shapeId;
	const btCollisionObject * m_exclude;
		
	virtual	btScalar	addSingleResult(btCollisionWorld::LocalRayResult& rayResult, bool normalInWorldSpace)
	{
		if (rayResult.m_collisionObject == m_exclude)
			return 1.0;
			
		//  fluid triggers - no collision
		if (rayResult.m_collisionObject->getCollisionFlags() & btCollisionObject::CF_NO_CONTACT_RESPONSE)
			return 1.0;
		
		//caller already does the filter on the m_closestHitFraction
		btAssert(rayResult.m_hitFraction <= m_closestHitFraction);
		
		m_closestHitFraction = rayResult.m_hitFraction;
		m_collisionObject = rayResult.m_collisionObject;

		if (!rayResult.m_localShapeInfo)
			m_shapeId = 0;  //crash hf-
		else  // only for btTriangleMeshShape
			m_shapeId = rayResult.m_localShapeInfo->m_shapePart;

		if (normalInWorldSpace)
		{
			m_hitNormalWorld = rayResult.m_hitNormalLocal;
		}
		else
		{
			///need to transform normal into worldspace
			m_hitNormalWorld = m_collisionObject->getWorldTransform().getBasis()*rayResult.m_hitNormalLocal;
		}
		m_hitPointWorld.setInterpolate3(m_rayFromWorld,m_rayToWorld,rayResult.m_hitFraction);
		return rayResult.m_hitFraction;
	}
};


//  Ray
//-------------------------------------------------------------------------------------------------------------------------------
bool COLLISION_WORLD::CastRay(
	const MATHVECTOR <float, 3> & origin,
	const MATHVECTOR <float, 3> & direction,
	const float length,
	const btCollisionObject * caster,
	COLLISION_CONTACT & contact,
	int* pOnRoad) const
{
	btVector3 from = ToBulletVector(origin);
	btVector3 to = ToBulletVector(origin + direction * length);
	MyRayResultCallback rayCallback(from, to, caster);
	
	MATHVECTOR <float, 3> p, n;  float d;
	const TRACKSURFACE * s = TRACKSURFACE::None();
	const BEZIER * b = NULL;
	btCollisionObject * c = NULL;
	
	// track geometry collision
	world->rayTest(from, to, rayCallback);
	bool geometryHit = rayCallback.hasHit();
	if (geometryHit)
	{
		p = ToMathVector<float>(rayCallback.m_hitPointWorld);
		n = ToMathVector<float>(rayCallback.m_hitNormalWorld);
		d = rayCallback.m_closestHitFraction * length;
		c = rayCallback.m_collisionObject;
		if (c->isStaticObject() /*&& (c->getCollisionFlags() & btCollisionObject::CF_NO_CONTACT_RESPONSE == 0)*/)
		{
			void * ptr = c->getCollisionShape()->getUserPointer();
			if (ptr == (void*)7777)  // road
			{
				s = trackSurface[0];
				*pOnRoad = 1;
			}
			if (ptr == (void*)7788)  // pipe
			{
				s = trackSurface[0];
				*pOnRoad = 2;
			}
			else if (ptr == 0)
			{
				*pOnRoad = 0;
				void * ptr = c->getUserPointer();
				if (ptr != NULL)
				{
					const TRACK_OBJECT * const obj = reinterpret_cast <const TRACK_OBJECT * const> (ptr);
					assert(obj);
					s = obj->GetSurface();
				}
				else //track geometry
				{
					int shapeId = rayCallback.m_shapeId;
					//assert(shapeId >= 0 && shapeId < trackSurface.size());
					if (shapeId >= trackSurface.size() || shapeId < 0)  shapeId = 0;  //crash hf-
					//if (trackSurface.size() > 0)
						s = trackSurface[shapeId];
				}
			}
		}
		
		// track bezierpatch collision
		if (track != NULL)
		{
			MATHVECTOR <float, 3> bezierspace_raystart(origin[1], origin[2], origin[0]);
			MATHVECTOR <float, 3> bezierspace_dir(direction[1], direction[2], direction[0]);
			MATHVECTOR <float, 3> colpoint, colnormal;
			const BEZIER * colpatch = NULL;
			bool bezierHit = track->CastRay(bezierspace_raystart, bezierspace_dir, length, colpoint, colpatch, colnormal);
			if (bezierHit)
			{
				p = MATHVECTOR <float, 3> (colpoint[2], colpoint[0], colpoint[1]);
				n = MATHVECTOR <float, 3> (colnormal[2], colnormal[0], colnormal[1]);
				d = (colpoint - bezierspace_raystart).Magnitude();
				s = track->GetRoadSurface();
				b = colpatch;
				c = NULL;
				*pOnRoad = 1;
			}
		}

		contact.Set(p, n, d, s, b, c);
		return true;
	}
	
	// should only happen on vehicle rollover
	contact.Set(origin + direction * length, -direction, length, s, b, c);
	return false;
}


//  Update
//-------------------------------------------------------------------------------------------------------------------------------
void DynamicsWorld::solveConstraints(btContactSolverInfo& solverInfo)
{
	btDiscreteDynamicsWorld::solveConstraints(solverInfo);
	//vHits.clear();
	//inFluids.clear();  //- before update

	///  collision callback
	//  and bullet hit info for particles and sounds ...
	int numManifolds = getDispatcher()->getNumManifolds();
	//LogO(toStr(numManifolds));
	for (int i=0; i < numManifolds; ++i)  // pairs
	{
		btPersistentManifold* contactManifold =  getDispatcher()->getManifoldByIndexInternal(i);
		btCollisionObject* bA = static_cast<btCollisionObject*>(contactManifold->getBody0());
		btCollisionObject* bB = static_cast<btCollisionObject*>(contactManifold->getBody1());
	
		/*if (bA->getCollisionFlags() & btCollisionObject::CF_NO_CONTACT_RESPONSE ||
			bB->getCollisionFlags() & btCollisionObject::CF_NO_CONTACT_RESPONSE) /*triggers*/

		void* pA = bA->getUserPointer(), *pB = bB->getUserPointer();
		//if (pA && pB)
		{
			ShapeData* sdA = (ShapeData*)pA, *sdB = (ShapeData*)pB, *sdCar=0, *sdFluid=0, *sdWheel=0;
			if (sdA) {  if (sdA->type == ST_Car)  sdCar = sdA;  else if (sdA->type == ST_Fluid)  sdFluid = sdA;  else if (sdA->type == ST_Wheel)  sdWheel = sdA;  }
			if (sdB) {  if (sdB->type == ST_Car)  sdCar = sdB;  else if (sdB->type == ST_Fluid)  sdFluid = sdB;  else if (sdB->type == ST_Wheel)  sdWheel = sdB;  }

			if (sdWheel)  /// wheel - fluid  -----~~~------~~~-----
			{
				if (sdFluid)
				{
					std::list<FluidBox*>& fl = sdWheel->pCarDyn->inFluidsWh[sdWheel->whNum];
					if (fl.empty())
						fl.push_back(sdFluid->pFluid);  // add fluid to wheel (only 1)
					//LogO("Wheel+ Fluid " + toStr(sdWheel->whNum));
				}
			}else
			if (sdCar)
				if (sdFluid)  /// car - fluid  -----~~~------~~~-----
				{
					if (sdCar->pCarDyn->inFluids.empty())
						sdCar->pCarDyn->inFluids.push_back(sdFluid->pFluid);  // add fluid to car (only 1)
					
					/*int numContacts = contactManifold->getNumContacts();
					//if (numContacts > 0)  LogO("c"+toStr(numContacts));
					for (int j=0; j < numContacts; ++j)
					{
						btManifoldPoint& pt = contactManifold->getContactPoint(j);
						//LogO("Car-Fluid " + toStr(pt.m_index0) + " " + toStr(pt.m_index1) + " " + toStr(pt.m_partId0) + " " + toStr(pt.m_partId1));
					}/**/
				}
				else  ///  car hit
				{
					int numContacts = contactManifold->getNumContacts();
					//if (numContacts > 0)  LogO("c"+toStr(numContacts));
					for (int j=0; j < numContacts; ++j)
					{
						btManifoldPoint& pt = contactManifold->getContactPoint(j);
						//LogO(Ogre::String("hit-")+toStr(i)+"-"+toStr(j)+" f "+toStr(f));
						DynamicsWorld::Hit hit;
						hit.pos = pt.getPositionWorldOnA();  hit.norm = pt.m_normalWorldOnB;
						hit.force = pt.getAppliedImpulse();  hit.sdCar = sdCar;
						hit.force = std::max(0, 60 - pt.getLifeTime());
						hit.vel = sdCar->pCarDyn->velPrev;
						vHits.push_back(hit);
						//sdCar->pCarDyn->hitPnts.push_back(pt);  ///i
					}
				}
		}
	}
}


void COLLISION_WORLD::Update(float dt, bool profiling)
{
	///  Simulate
	world->stepSimulation(dt, maxSubsteps, fixedTimestep);
	

	//  use collision hit results, once a frame
	
	int n = world->vHits.size();
	if (n > 0)
	{
		//LogO(toStr(n));
		//  pick the one with biggest force
		DynamicsWorld::Hit& hit = world->vHits[0];
		float force = 0.f;//, vel = 0.f;
		for (int i=0; i < n; ++i)
			if (world->vHits[i].force > force)
			{
				force = world->vHits[i].force;
				hit = world->vHits[i];
			}

		CARDYNAMICS* cd = hit.sdCar->pCarDyn;
		btVector3 vcar = hit.vel;
		Ogre::Vector3 vel(vcar[0], vcar[2], -vcar[1]);
		Ogre::Vector3 norm(hit.norm.getX(), hit.norm.getZ(), -hit.norm.getY());
		float vlen = vel.length(), normvel = abs(vel.dotProduct(norm));

		//  Sparks emit params
		cd->vHitPos = Ogre::Vector3(hit.pos.getX(), hit.pos.getZ(), -hit.pos.getY());
		cd->vHitNorm = norm + vel * 0.1f;  //vel*0.2 + norm*3;
		cd->fParVel = 3.0 + 0.4 * vlen;
		cd->fParIntens = 10.f + 30.f * vlen;  //hit.force * std::min(1.f, vlen);

		//if (vlen > 2.f)// && car)  //par
		{
			cd->fSndForce = normvel*0.02 + 0.02*vlen;  //hit.force;
			cd->fHitTime = 1.f;  cd->fNormVel = normvel;
			///LogO("upd sf " + toStr(cd->fSndForce) + " force " + toStr(hit.force) + " vel " + toStr(vlen) + " Nvel " + toStr(normvel));

			//int f = (normvel*0.02f + 0.02f*vlen) * Ncrashsounds;
			//int i = std::max(5, std::min(Ncrashsounds, f));
			//cd->bHitSnd = true;//cd->fSndForce > 58;  //true;
			//cd->sndHitN = i;
		}
	}
	world->vHits.clear();//+


	///+  bullet profiling info
	static int cc = 0;  cc++;
	if (cc > 40)
	{	cc = 0;
		if (profiling)
		{
			std::stringstream os;
			CProfileManager::dumpAll(os);
			bltProfiling = os.str();
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------------------


void COLLISION_WORLD::DebugPrint(std::ostream & out)
{
	out << "Collision objects: " << world->getNumCollisionObjects() << std::endl;
}

//  Clear - delete bullet pointers
void COLLISION_WORLD::Clear()
{
	track = NULL;
	if(trackObject)
	{
		delete trackObject->getCollisionShape();
		trackObject = NULL;
	}
	if(trackMesh)
	{
		delete trackMesh;
		trackMesh = NULL;
	}
	trackSurface.resize(0);

	// remove constraint before deleting rigid body
	for(int i = 0; i < constraints.size(); i++)
	{
		world->removeConstraint(constraints[i]);
		delete constraints[i];
	}
	constraints.resize(0);
	
	for(int i = world->getNumCollisionObjects() - 1; i >= 0; i--)
	{
		btCollisionObject* obj = world->getCollisionObjectArray()[i];
		btRigidBody* body = btRigidBody::upcast(obj);
		if (body && body->getMotionState())
		{
			delete body->getMotionState();
		}
		world->removeCollisionObject(obj);

		delete obj->getUserPointer();  // ShapeData
		delete obj;
	}
	
	for(int i = 0; i < shapes.size(); i++)
	{
		btCollisionShape * shape = shapes[i];
		if (shape->isCompound())
		{
			btCompoundShape * cs = (btCompoundShape *)shape;
			for (int i = 0; i < cs->getNumChildShapes(); i++)
			{
				delete cs->getChildShape(i);
			}
		}
		delete shape;
	}
	shapes.resize(0);
	
	for(int i = 0; i < meshes.size(); i++)
	{
		delete meshes[i];
	}
	meshes.resize(0);
	
	for(int i = 0; i < actions.size(); i++)
	{
		world->removeAction(actions[i]);
	}
	actions.resize(0);
}

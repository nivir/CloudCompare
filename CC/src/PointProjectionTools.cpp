//##########################################################################
//#                                                                        #
//#                               CCLIB                                    #
//#                                                                        #
//#  This program is free software; you can redistribute it and/or modify  #
//#  it under the terms of the GNU Library General Public License as       #
//#  published by the Free Software Foundation; version 2 of the License.  #
//#                                                                        #
//#  This program is distributed in the hope that it will be useful,       #
//#  but WITHOUT ANY WARRANTY; without even the implied warranty of        #
//#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         #
//#  GNU General Public License for more details.                          #
//#                                                                        #
//#          COPYRIGHT: EDF R&D / TELECOM ParisTech (ENST-TSI)             #
//#                                                                        #
//##########################################################################

#include "PointProjectionTools.h"

//local
#include "SimpleCloud.h"
#include "Delaunay2dMesh.h"
#include "GenericIndexedMesh.h"
#include "GenericProgressCallback.h"
#include "Neighbourhood.h"
#include "SimpleMesh.h"

//system
#include <assert.h>

using namespace CCLib;

SimpleCloud* PointProjectionTools::developCloudOnCylinder(GenericCloud* theCloud,
															PointCoordinateType radius,
															unsigned char dim,
															CCVector3* center,
															GenericProgressCallback* progressCb)
{
	if (!theCloud)
		return 0;

	uchar dim1 = (dim>0 ? dim-1 : 2);
	uchar dim2 = (dim<2 ? dim+1 : 0);

	unsigned count = theCloud->size();

	SimpleCloud* newList = new SimpleCloud();
	if (!newList->reserve(count)) //not enough memory
		return 0;

	//we compute cloud bounding box center if no center is specified
	CCVector3 C;
	if (!center)
	{
		PointCoordinateType Mins[3],Maxs[3];
		theCloud->getBoundingBox(Mins,Maxs);
		C = (CCVector3(Mins)+CCVector3(Maxs))*0.5;
		center = &C;
	}

	NormalizedProgress* nprogress = 0;
	if (progressCb)
	{
		progressCb->reset();
		progressCb->setMethodTitle("Develop");
		char buffer[256];
		sprintf(buffer,"Number of points = %i",count);
		nprogress = new NormalizedProgress(progressCb,count);
		progressCb->setInfo(buffer);
		progressCb->start();
	}

	const CCVector3 *Q;
	CCVector3 P;
	PointCoordinateType u,lon;

	theCloud->placeIteratorAtBegining();
	while ((Q = theCloud->getNextPoint()))
	{
		P = *Q-*center;
		u = sqrt(P.u[dim1] * P.u[dim1] + P.u[dim2] * P.u[dim2]);
		lon = atan2(P.u[dim1],P.u[dim2]);

		newList->addPoint(CCVector3(lon*radius,P.u[dim],u-radius));

		if (nprogress)
		{
			if (!nprogress->oneStep())
				break;
		}

	}

	if (progressCb)
	{
		delete nprogress;
		progressCb->stop();
	}

	return newList;
}

//deroule la liste sur un cone dont le centre est "center" et d'angle alpha en degres
SimpleCloud* PointProjectionTools::developCloudOnCone(GenericCloud* theCloud, uchar dim, PointCoordinateType baseRadius, float alpha, const CCVector3& center, GenericProgressCallback* progressCb)
{
	if (!theCloud)
		return 0;

	unsigned count=theCloud->size();

	SimpleCloud* cloud = new SimpleCloud();
	if (!cloud->reserve(count)) //not enough memory
		return 0;

	uchar dim1 = (dim>0 ? dim-1 : 2);
	uchar dim2 = (dim<2 ? dim+1 : 0);

	float tan_alpha = tan(alpha*(float)(CC_DEG_TO_RAD));
	//float cos_alpha = cos(alpha*CC_DEG_TO_RAD);
	//float sin_alpha = sin(alpha*CC_DEG_TO_RAD);
	float q = 1.0f/(1.0f+tan_alpha*tan_alpha);

	CCVector3 P;
	PointCoordinateType u,lon,z2,x2,dX,dZ,lat,alt;

	theCloud->placeIteratorAtBegining();
	//normsType* _theNorms = theNorms.begin();

	NormalizedProgress* nprogress = 0;
	if (progressCb)
	{
		progressCb->reset();
		progressCb->setMethodTitle("DevelopOnCone");
		char buffer[256];
		sprintf(buffer,"Number of points = %i",count);
		nprogress = new NormalizedProgress(progressCb,count);
		progressCb->setInfo(buffer);
		progressCb->start();
	}

	for (unsigned i=0;i<count;i++)
	{
		const CCVector3 *Q = theCloud->getNextPoint();
		P = *Q-center;

		u = sqrt(P.u[dim1]*P.u[dim1] + P.u[dim2]*P.u[dim2]);
		lon = atan2(P.u[dim1],P.u[dim2]);

		//projection sur le cone
		z2 = (P.u[dim]+u*tan_alpha)*q;
		x2 = z2*tan_alpha;
		//ordonnee
		//#define ORTHO_CONIC_PROJECTION
		#ifdef ORTHO_CONIC_PROJECTION
		lat = sqrt(x2*x2+z2*z2)*cos_alpha;
		if (lat*z2<0.0) lat=-lat;
		#else
		lat = P.u[dim];
		#endif
		//altitude
		dX = u-x2;
		dZ = P.u[dim]-z2;
		alt = sqrt(dX*dX+dZ*dZ);
		//on regarde de quel cote de la surface du cone le resultat tombe par p.v.
		if (x2*P.u[dim] - z2*u<0.0)
			alt=-alt;

		cloud->addPoint(CCVector3(lon*baseRadius,lat+center[dim],alt));

		if (progressCb)
		{
			if (!nprogress->oneStep())
				break;
		}
	}

	if (progressCb)
	{
		delete nprogress;
		progressCb->stop();
	}

	return cloud;
}

SimpleCloud* PointProjectionTools::applyTransformation(GenericCloud* theCloud, Transformation& trans, GenericProgressCallback* progressCb)
{
    assert(theCloud);

    unsigned count = theCloud->size();

    SimpleCloud* transformedCloud = new SimpleCloud();
    if (!transformedCloud->reserve(count))
        return 0; //not enough memory

    NormalizedProgress* nprogress = 0;
    if (progressCb)
    {
        progressCb->reset();
        progressCb->setMethodTitle("ApplyTransformation");
        nprogress = new NormalizedProgress(progressCb,count);
        char buffer[256];
        sprintf(buffer,"Number of points = %i",count);
        progressCb->setInfo(buffer);
        progressCb->start();
    }

    theCloud->placeIteratorAtBegining();
    const CCVector3* P;

	if (trans.R.isValid())
	{
		while ((P = theCloud->getNextPoint()))
		{
			//P' = s*R.P+T
			CCVector3 newP = trans.s * (trans.R * (*P)) + trans.T;

			transformedCloud->addPoint(newP);

			if (nprogress && !nprogress->oneStep())
				break;
		}
	}
	else
	{
		while ((P = theCloud->getNextPoint()))
		{
			//P' = s*P+T
			CCVector3 newP = trans.s * (*P) + trans.T;

			transformedCloud->addPoint(newP);

			if (nprogress && !nprogress->oneStep())
				break;
		}
	}

    if (progressCb)
        progressCb->stop();

    return transformedCloud;
}

GenericIndexedMesh* PointProjectionTools::computeTriangulation(	GenericIndexedCloudPersist* theCloud,
																CC_TRIANGULATION_TYPES type/*=GENERIC*/,
																PointCoordinateType maxEdgeLength/*=0*/)
{
	if (!theCloud)
		return 0;

	//output mesh
	GenericIndexedMesh* theMesh = 0;

	switch(type)
	{
	case GENERIC:
		{
			unsigned count = theCloud->size();
			CC2DPointsContainer the2DPoints;
			try
			{
				the2DPoints.resize(count);
			}
			catch (.../*const std::bad_alloc&*/) //out of memory
			{
				break;
			}

			theCloud->placeIteratorAtBegining();
			CCVector3 P;
			for (unsigned i=0; i<count; ++i)
			{
				theCloud->getPoint(i,P);
				the2DPoints[i].x = P.x;
				the2DPoints[i].y = P.y;
			}

			Delaunay2dMesh* dm = new Delaunay2dMesh();
			if (!dm->build(the2DPoints,0))
			{
				delete dm;
				return 0;
			}
			dm->linkMeshWith(theCloud,false);

			//remove triangles with too long edges
			if (maxEdgeLength > 0)
			{
				dm->removeTrianglesLongerThan(maxEdgeLength);
				if (dm->size() == 0)
				{
					//no more triangles?
					delete dm;
					dm = 0;
				}
			}

			theMesh = static_cast<GenericIndexedMesh*>(dm);
		}
		break;
	case GENERIC_BEST_LS_PLANE:
		{
			Neighbourhood Yk(theCloud);
			theMesh = Yk.triangulateOnPlane(false,maxEdgeLength);
		}
		break;
	case GENERIC_EMPTY:
		theMesh = new SimpleMesh(theCloud);
		break;
	}

	return theMesh;
}

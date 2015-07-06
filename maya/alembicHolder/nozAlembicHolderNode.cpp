/*Alembic Holder
Copyright (c) 2014, Ga�l Honorez, All rights reserved.
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3.0 of the License, or (at your option) any later version.
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
Lesser General Public License for more details.
You should have received a copy of the GNU Lesser General Public
License along with this library.*/


#include <maya/MCommandResult.h>

#include "nozAlembicHolderNode.h"
#include "parseJsonShaders.h"

#include "gpuCacheGLPickingSelect.h"
#include "gpuCacheRasterSelect.h"

#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MGlobal.h>
#include <maya/MTime.h>

#include <maya/MDrawData.h>
#include <maya/MMaterial.h>
#include <maya/MSelectionMask.h>
#include <maya/MSelectionList.h>
#include <maya/MRenderView.h>
#include <maya/MViewport2Renderer.h>
#include <maya/MHWGeometryUtilities.h>
#include <maya/MFnMessageAttribute.h>
#include <maya/MFileObject.h>
#include <maya/MSelectionList.h>
#include <maya/MRenderView.h>
#include <maya/MViewport2Renderer.h>
#include <maya/MHWGeometryUtilities.h>
#include <maya/MFnMessageAttribute.h>
#include <maya/MFileObject.h>
#include <stdio.h>
#include <map>
#include <algorithm>


#include <maya/MHardwareRenderer.h>
#include <maya/MGLFunctionTable.h>

// Wireframe line style defines
#define LINE_STIPPLE_SHORTDASHED    0x0303

#define LEAD_COLOR                18    // green

#include <stdio.h>
#include <map>

#define LEAD_COLOR                18    // green
#define ACTIVE_COLOR            15    // white
#define ACTIVE_AFFECTED_COLOR    8    // purple
#define DORMANT_COLOR            4    // blue
#define HILITE_COLOR            17    // pale blue
//#include "timer.h"

//
static MGLFunctionTable *gGLFT = NULL;

//#include "boost/foreach.hpp"

typedef std::map<std::string, GLuint> NodeCache;

NodeCache g_bboxCache;


// The id is a 32bit value used to identify this type of node in the binary file format.
MTypeId nozAlembicHolder::id(0x00114956);

MObject nozAlembicHolder::aAbcFile;
MObject nozAlembicHolder::aObjectPath;
MObject nozAlembicHolder::aSelectionPath;
MObject nozAlembicHolder::aBoundingExtended;
MObject nozAlembicHolder::aTime;
MObject nozAlembicHolder::aTimeOffset;
MObject nozAlembicHolder::aShaderPath;
MObject nozAlembicHolder::aUpdateCache;
MObject nozAlembicHolder::aForceReload;

// for bbox
MObject nozAlembicHolder::aBoundMin;
MObject nozAlembicHolder::aBoundMinX;
MObject nozAlembicHolder::aBoundMinY;
MObject nozAlembicHolder::aBoundMinZ;
MObject nozAlembicHolder::aBoundMax;
MObject nozAlembicHolder::aBoundMaxX;
MObject nozAlembicHolder::aBoundMaxY;
MObject nozAlembicHolder::aBoundMaxZ;


SimpleAbcViewer::SceneState CAlembicDatas::abcSceneState;
SimpleAbcViewer::SceneManager CAlembicDatas::abcSceneManager;

CAlembicDatas::CAlembicDatas() {

    bbox = MBoundingBox(MPoint(-1.0f, -1.0f, -1.0f), MPoint(1.0f, 1.0f, 1.0f));
    token = 0;
    m_bbextendedmode = false;
    m_currscenekey = "";
	m_currselectionkey = "";
    m_abcdirty = false;

}

void abcDirtiedCallback(MObject & nodeMO, MPlug & plug, void* data) {

    const nozAlembicHolder& node = *(nozAlembicHolder*)data;

    //nozAlembicHolder::nozAlembicHolder *node = (nozAlembicHolder::nozAlembicHolder *) data;
    //cout << "plug.attribute()" << plug.partialName(false, false, false, false, false, true) << endl;

    MFnDagNode fn( nodeMO );
    if (plug.partialName(false, false, false, false, false, true) == "abcFile" || plug.partialName(false, false, false, false, false, true) == "objectPath")
    {
        MFnDagNode fn(node.thisMObject());
        //std::cout << "plugDirtied: " << fn.name() << " " << plug.name() << " = " << plug.asString() << std::endl;
        node.boundingBox();

    }
}

void abcChangedCallback(MNodeMessage::AttributeMessage msg, MPlug & plug,
        MPlug & otherPlug, void* data) {
}

void nodePreRemovalCallback(MObject& obj, void* data) {
    std::string sceneKey = ((nozAlembicHolder*) data)->getSceneKey();
    CAlembicDatas::abcSceneManager.removeScene(sceneKey);
    if(CAlembicDatas::abcSceneManager.hasKey(sceneKey) == 0)
    {
        NodeCache::iterator J = g_bboxCache.find(sceneKey);
        if (J != g_bboxCache.end())
        {
            glDeleteLists((*J).second, 1);
            g_bboxCache.erase(J);
        }

    }
}

nozAlembicHolder::nozAlembicHolder() {
        //std::cout << "class creator" << std::endl;
}

nozAlembicHolder::~nozAlembicHolder() {
}

void nozAlembicHolder::setHolderTime() const {

    const CAlembicDatas* geom = const_cast<nozAlembicHolder*> (this)->alembicData();

    if(geom != NULL)
    {
        MFnDagNode fn(thisMObject());
        MTime time, timeOffset;

        MPlug plug = fn.findPlug(aTime);
        plug.getValue(time);
        plug = fn.findPlug(aTimeOffset);
        plug.getValue(timeOffset);

        double dtime;

        dtime = time.as(MTime::kSeconds) + timeOffset.as(MTime::kSeconds);

        std::string sceneKey = getSceneKey();
        if (geom->abcSceneManager.hasKey(sceneKey))
            geom->abcSceneManager.getScene(sceneKey)->setTime(dtime);
    }
}


bool nozAlembicHolder::isBounded() const 
{
    return true;
}



MBoundingBox nozAlembicHolder::boundingBox() const
{
	CAlembicDatas* geom = const_cast<nozAlembicHolder*> (this)->alembicData();
    MBoundingBox bbox = MBoundingBox(MPoint(-1.0f, -1.0f, -1.0f), MPoint(1.0f, 1.0f, 1.0f));

    if(geom != NULL)
        bbox = geom->bbox;

    return bbox;
}

void* nozAlembicHolder::creator() 
{
    return new nozAlembicHolder();
}

void nozAlembicHolder::postConstructor() 
{
    // This call allows the shape to have shading groups assigned
    setRenderable(true);

    // callbacks
    MObject node = thisMObject();
    MNodeMessage::addAttributeChangedCallback(node, abcChangedCallback, this);
    MNodeMessage::addNodePreRemovalCallback(node, nodePreRemovalCallback, this);


}

void nozAlembicHolder::copyInternalData(MPxNode* srcNode) {
    // here we ensure that the scene manager stays up to date when duplicating nodes

    const nozAlembicHolder& node = *(nozAlembicHolder*)srcNode;

    CAlembicDatas* geom = const_cast<nozAlembicHolder*> (this)->alembicData();

    MFnDagNode fn(node.thisMObject());
    MString abcfile;
    MPlug plug = fn.findPlug(aAbcFile);
    plug.getValue(abcfile);
    abcfile = abcfile.expandFilePath();
    MString objectPath;
    plug = fn.findPlug(aObjectPath);
    plug.getValue(objectPath);

    MString selectionPath;
    plug = fn.findPlug(aSelectionPath);
    plug.getValue(selectionPath);

    bool extendedMode = false;
    plug = fn.findPlug(aBoundingExtended);
    plug.getValue(extendedMode);

    if(geom != NULL)
    {
        geom->abcSceneManager.addScene(abcfile.asChar(), objectPath.asChar());
        geom->m_currscenekey = getSceneKey();
		geom->m_currselectionkey = getSelectionKey();
        geom->m_bbextendedmode = extendedMode;
    }

}

MStatus nozAlembicHolder::initialize() {
    // init maya stuff

    MFnTypedAttribute tAttr;
    MFnNumericAttribute nAttr;
    MFnMessageAttribute mAttr;
    MFnUnitAttribute uAttr;
    MStatus stat;

    aAbcFile = tAttr.create("cacheFileName", "cfn", MFnStringData::kString, MObject::kNullObj);
    tAttr.setWritable(true);
    tAttr.setReadable(true);
    tAttr.setHidden(false);
    tAttr.setStorable(true);
    //tAttr.setUsedAsFilename(true);
    tAttr.setKeyable(true);

    aObjectPath = tAttr.create("cacheGeomPath", "cmp", MFnStringData::kString);
    tAttr.setWritable(true);
    tAttr.setReadable(true);
    tAttr.setHidden(false);
    tAttr.setStorable(true);
    tAttr.setKeyable(true);

    aSelectionPath = tAttr.create("cacheSelectionPath", "csp", MFnStringData::kString);
    tAttr.setWritable(true);
    tAttr.setReadable(true);
    tAttr.setHidden(false);
    tAttr.setStorable(true);
    tAttr.setKeyable(true);

    aSelectionPath = tAttr.create("cacheSelectionPath", "csp", MFnStringData::kString);
    tAttr.setWritable(true);
    tAttr.setReadable(true);
    tAttr.setHidden(false);
    tAttr.setStorable(true);
    tAttr.setKeyable(true);

    aShaderPath = mAttr.create("shaders", "shds", &stat);
    //    tAttr.setWritable(true);
    //    tAttr.setReadable(true);
    mAttr.setArray(true);
    mAttr.setInternal(true);

    aTime = uAttr.create("time", "t", MFnUnitAttribute::kTime);
    uAttr.setStorable(false);
    uAttr.setKeyable(true);
    uAttr.setReadable(true);
    uAttr.setWritable(true);

    aTimeOffset = uAttr.create("timeOffset", "to", MFnUnitAttribute::kTime);
    uAttr.setStorable(true);
    uAttr.setKeyable(true);

    aBoundingExtended = nAttr.create("boundingBoxExtendedMode", "bem", MFnNumericData::kBoolean, false);
    nAttr.setWritable(true);
    nAttr.setReadable(true);
    nAttr.setHidden(false);
    nAttr.setStorable(true);
    nAttr.setKeyable(true);

    aUpdateCache = nAttr.create("updateCache", "upc", MFnNumericData::kInt, 0);
    nAttr.setHidden(true);

    aForceReload = nAttr.create("forceReload", "frel", MFnNumericData::kBoolean, false, &stat);
    nAttr.setDefault(false);
    nAttr.setKeyable(false);

    aBoundMinX = nAttr.create( "outBoundMinX", "obminx", MFnNumericData::kFloat, 0, &stat);
    nAttr.setWritable( false );
    nAttr.setStorable( false );

    aBoundMinY = nAttr.create( "outBoundMinY", "cobminy", MFnNumericData::kFloat, 0, &stat );
    nAttr.setWritable( false );
    nAttr.setStorable( false );

    aBoundMinZ = nAttr.create( "outBoundMinZ", "obminz", MFnNumericData::kFloat, 0, &stat );
    nAttr.setWritable( false );
    nAttr.setStorable( false );

    aBoundMin = nAttr.create( "outBoundMin", "obmin", aBoundMinX, aBoundMinY, aBoundMinZ, &stat );
    nAttr.setWritable( false );
    nAttr.setStorable( false );

    aBoundMaxX = nAttr.create( "outBoundMaxX", "obmaxx", MFnNumericData::kFloat, 0, &stat );
    nAttr.setWritable( false );
    nAttr.setStorable( false );

    aBoundMaxY = nAttr.create( "outBoundMaxY", "cobmaxy", MFnNumericData::kFloat, 0, &stat );
    nAttr.setWritable( false );
    nAttr.setStorable( false );

    aBoundMaxZ = nAttr.create( "outBoundMaxZ", "obmaxz", MFnNumericData::kFloat, 0, &stat );
    nAttr.setWritable( false );
    nAttr.setStorable( false );

    aBoundMax = nAttr.create( "outBoundMax", "obmax", aBoundMaxX, aBoundMaxY, aBoundMaxZ, &stat );
    nAttr.setWritable( false );
    nAttr.setStorable( false );

    // Add the attributes we have created to the node
    //
    addAttribute(aAbcFile);
    addAttribute(aObjectPath);
    addAttribute(aSelectionPath);
    addAttribute(aBoundingExtended);
    addAttribute(aForceReload);
    addAttribute(aShaderPath);
    addAttribute(aTime);
    addAttribute(aTimeOffset);
    addAttribute(aUpdateCache);
    addAttribute(aBoundMin);
    addAttribute(aBoundMax);

    attributeAffects ( aForceReload, aUpdateCache );
    attributeAffects ( aBoundingExtended, aUpdateCache );
    attributeAffects ( aAbcFile, aUpdateCache );
    attributeAffects ( aObjectPath, aUpdateCache );
    attributeAffects ( aSelectionPath, aUpdateCache );
    attributeAffects ( aTime, aUpdateCache );
    attributeAffects ( aTimeOffset, aUpdateCache );

    attributeAffects ( aUpdateCache, aBoundMin );
    attributeAffects ( aUpdateCache, aBoundMax );


    return MS::kSuccess;

}

std::string nozAlembicHolder::getSelectionKey() const {
    MFnDagNode fn(thisMObject());
    MString selectionPath;
    MPlug plug = fn.findPlug(aSelectionPath);
    plug.getValue(selectionPath);
    return std::string(selectionPath.asChar());
}

std::string nozAlembicHolder::getSceneKey() const {
    MFnDagNode fn(thisMObject());
    MString abcfile;
    MPlug plug = fn.findPlug(aAbcFile);
    plug.getValue(abcfile);

    MFileObject fileObject;
    fileObject.setRawFullName(abcfile.expandFilePath());
    fileObject.setResolveMethod(MFileObject::kInputFile);
    abcfile = fileObject.resolvedFullName();

    MString objectPath;
    plug = fn.findPlug(aObjectPath);
    plug.getValue(objectPath);
    return std::string((abcfile + "|" + objectPath).asChar());
}


MStatus nozAlembicHolder::compute( const MPlug& plug, MDataBlock& block )
{
    if (plug == aUpdateCache)
    {
        MStatus status;
        MFnDagNode fn(thisMObject());
        // Try to parse JSON Shader assignation here.
        MPlug shaderAssignation = fn.findPlug("shadersAssignation", &status);

        if(status == MS::kSuccess)
        {
            bool parsingSuccessful = false;
            MString jsonAssign = block.inputValue(shaderAssignation).asString();
            if(jsonAssign != "")
            {
                Json::Value jroot;
                Json::Reader reader;
                parsingSuccessful = reader.parse( jsonAssign.asChar(), jroot, false );
                if(parsingSuccessful)
                    ParseShaders(jroot, fGeometry.shaderColors);
            }
        }

        MString file = block.inputValue(aAbcFile).asString();
        MFileObject fileObject;
        fileObject.setRawFullName(file.expandFilePath());
        fileObject.setResolveMethod(MFileObject::kInputFile);
        file = fileObject.resolvedFullName();

        MString objectPath = block.inputValue(aObjectPath).asString();
        MString selectionPath = block.inputValue(aSelectionPath).asString();

		fGeometry.m_currselectionkey = selectionPath.asChar();

        MTime time = block.inputValue(aTime).asTime() + block.inputValue(aTimeOffset).asTime(); 

        fGeometry.m_bbextendedmode = block.inputValue(aBoundingExtended).asBool();

        bool hasToReload = false;

        if (fGeometry.time != time.value())
        {
            fGeometry.m_abcdirty = true;
            fGeometry.time = time.value();
            hasToReload = true;
        }

        MString mkey = file + "|" + objectPath;

        std::string key = mkey.asChar();

        if ((fGeometry.m_currscenekey != key && mkey != "|" ) || hasToReload)
        {
            if (fGeometry.m_currscenekey != key && mkey != "|" )
            {
                CAlembicDatas::abcSceneManager.removeScene(fGeometry.m_currscenekey);
                if(CAlembicDatas::abcSceneManager.hasKey(fGeometry.m_currscenekey) == 0)
                {
                    NodeCache::iterator J = g_bboxCache.find(fGeometry.m_currscenekey);
                    if (J != g_bboxCache.end())
                    {
                        glDeleteLists((*J).second, 1);
                        g_bboxCache.erase(J);
                    }

                }


                fGeometry.m_currscenekey = "";
                CAlembicDatas::abcSceneManager.addScene(file.asChar(), objectPath.asChar());
                fGeometry.m_currscenekey = key;
            }

            if (CAlembicDatas::abcSceneManager.hasKey(key))
            {
                fGeometry.bbox = MBoundingBox();
                SimpleAbcViewer::Box3d bb;
                setHolderTime();
                bb = CAlembicDatas::abcSceneManager.getScene(key)->getBounds();
                fGeometry.bbox.expand(MPoint(bb.min.x, bb.min.y, bb.min.z));
                fGeometry.bbox.expand(MPoint(bb.max.x, bb.max.y, bb.max.z));
                // notify viewport 2.0 that we are dirty
                MHWRender::MRenderer::setGeometryDrawDirty(thisMObject());

            }


            fGeometry.m_abcdirty = true;

        }

        block.outputValue(aForceReload).setBool(false);
    }
    else
    {
        return ( MS::kUnknownParameter );
    }

    block.setClean(plug);
    return MS::kSuccess;

}

bool nozAlembicHolder::GetPlugData()
{
    int update = 0;
    MPlug updatePlug(thisMObject(), aUpdateCache );
    updatePlug.getValue( update );
    if (update != dUpdate)
    {
        dUpdate = update;
        return true;
    }
    else
    {
        return false;
    }
    return false;

}

CAlembicDatas* nozAlembicHolder::alembicData()
{
    if (MRenderView::doesRenderEditorExist())
    {
        GetPlugData();
        return &fGeometry;
    }
    return NULL;
}

// UI IMPLEMENTATION

CAlembicHolderUI::~CAlembicHolderUI() {
}

void* CAlembicHolderUI::creator() {
    return new CAlembicHolderUI();
}

CAlembicHolderUI::CAlembicHolderUI() {
}
void CAlembicHolderUI::getDrawRequests(const MDrawInfo & info,
        bool /*objectAndActiveOnly*/, MDrawRequestQueue & queue) {

    if(MGlobal::mayaState() != MGlobal::kInteractive)
        return;

    MDrawData data;
    MDrawRequest request = info.getPrototype(*this);
    nozAlembicHolder* shapeNode = (nozAlembicHolder*) surfaceShape();
    CAlembicDatas* geom = shapeNode->alembicData();
    
    if(geom == NULL)
        return;

    getDrawData(geom, data);
    request.setDrawData(data);

    // Are we displaying meshes?
    if (!info.objectDisplayStatus(M3dView::kDisplayMeshes))
        return;

    // Use display status to determine what color to draw the object
    //
    switch (info.displayStyle()) {
    case M3dView::kWireFrame:
        getDrawRequestsWireFrame(request, info);
        queue.add(request);
        break;

    case M3dView::kGouraudShaded:
        request.setToken(kDrawSmoothShaded);
        getDrawRequestsShaded(request, info, queue, data);
        queue.add(request);
        break;

    case M3dView::kFlatShaded:
        request.setToken(kDrawFlatShaded);
        getDrawRequestsShaded(request, info, queue, data);
        queue.add(request);
        break;
    case M3dView::kBoundingBox :
            request.setToken( kDrawBoundingBox );
            getDrawRequestsBoundingBox(request, info);
            queue.add( request );
    default:
        break;
    }

    //cout << "Ending draw request" << endl;
}

void CAlembicHolderUI::draw(const MDrawRequest & request, M3dView & view) const
{

	

    int token = request.token();

    M3dView::DisplayStatus displayStatus = request.displayStatus();
    
    bool cacheSelected =  ((displayStatus == M3dView::kActive) || (displayStatus == M3dView::kLead) || (displayStatus == M3dView::kHilite));
    MDrawData data = request.drawData();

    nozAlembicHolder* shapeNode = (nozAlembicHolder*) surfaceShape();
    CAlembicDatas * cache = (CAlembicDatas*) data.geometry();

    bool refresh = cache->m_abcdirty;
    bool forceBoundingBox = (cache->m_bbextendedmode && cacheSelected == false);

    int oldToken = cache->token;

    cache->token = token;


    if(cache->m_currselectionkey.size() != 0)
        token = kDrawBoundingBox;
    else if(forceBoundingBox)
        token = kDrawBoundingBox;
	
	

    view.beginGL();

    // Setup the OpenGL state as necessary
    //
    // The most straightforward way to ensure that the OpenGL
    // material parameters are properly restored after drawing is
    // to use push/pop attrib as we have no easy of knowing the
    // current values of all the parameters.
    glPushAttrib(MGL_LIGHTING_BIT);

    // handling refreshes
    GLuint glcache = -1;
    if(token == kDrawBoundingBox)
    {
        NodeCache::iterator I = g_bboxCache.find(cache->m_currscenekey);
        if (I == g_bboxCache.end())
            refresh=true;
        else
            glcache = (*I).second;

        if(refresh == true)
        {
            if (glcache != -1)
            {
                glDeleteLists(glcache,1);
            }
            glcache = glGenLists(1);
            glNewList(glcache, MGL_COMPILE);
            drawBoundingBox( request, view );
            glEndList();
            g_bboxCache[cache->m_currscenekey] = glcache;
        }
    }

    switch (token)
    {
    case kDrawBoundingBox :
        glCallList(glcache);
        break;

    case kDrawWireframe:
    case kDrawWireframeOnShaded:
        if (cache->abcSceneManager.hasKey(cache->m_currscenekey))
        {
            gGLFT->glPolygonMode(GL_FRONT_AND_BACK, MGL_LINE);
            gGLFT->glPushMatrix();
			cache->abcSceneManager.getScene(cache->m_currscenekey)->draw(cache->abcSceneState, std::map<std::string, MColor>());
            gGLFT->glPopMatrix();
            gGLFT->glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        }
        else
            drawBoundingBox( request, view );
        break;
    case kDrawSmoothShaded:
    case kDrawFlatShaded:
        if (cache->abcSceneManager.hasKey(cache->m_currscenekey))
            drawingMeshes(cache->m_currscenekey, cache, "");
        else
            drawBoundingBox( request, view );
        break;
    }

    if(cache->m_currselectionkey.size() != 0 && cache->abcSceneManager.hasKey(cache->m_currscenekey))
    {
        switch (cache->token)
        {
        case kDrawBoundingBox :
            break;
        case kDrawWireframe:
        case kDrawWireframeOnShaded:
            gGLFT->glPolygonMode(GL_FRONT_AND_BACK, MGL_LINE);
            gGLFT->glPushMatrix();
            cache->abcSceneManager.getScene(cache->m_currscenekey)->drawOnly(cache->abcSceneState, cache->m_currselectionkey, std::map<std::string, MColor>());
            gGLFT->glPopMatrix();
            gGLFT->glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            break;
        case kDrawSmoothShaded:
        case kDrawFlatShaded:
            drawingMeshes(cache->m_currscenekey, cache, cache->m_currselectionkey);
            break;
        }
    }

    cache->m_abcdirty = false;
    glPopAttrib();
    view.endGL();
}

void CAlembicHolderUI::drawingMeshes( std::string sceneKey, CAlembicDatas * cache, std::string selectionKey) const
{
    gGLFT->glPushMatrix();

    glEnable(MGL_POLYGON_OFFSET_FILL);  // Viewport has set the offset, just enable it

    glColorMaterial(MGL_FRONT_AND_BACK, MGL_AMBIENT_AND_DIFFUSE);
    glEnable(MGL_COLOR_MATERIAL) ;
    glColor4f(.7, .7, .7, 1.0);
    // On Geforce cards, we emulate two-sided lighting by drawing
    // triangles twice because two-sided lighting is 10 times
    // slower than single-sided lighting.
        
    bool needEmulateTwoSidedLighting = false;
    // Query face-culling and two-sided lighting state          
    bool  cullFace = (gGLFT->glIsEnabled(MGL_CULL_FACE) == MGL_TRUE);
    MGLint twoSidedLighting = MGL_FALSE;
    gGLFT->glGetIntegerv(MGL_LIGHT_MODEL_TWO_SIDE, &twoSidedLighting);

    needEmulateTwoSidedLighting = (!cullFace && twoSidedLighting);

    if(needEmulateTwoSidedLighting)
    {
        glEnable(MGL_CULL_FACE);
        glLightModeli(MGL_LIGHT_MODEL_TWO_SIDE, 0);
        glCullFace(MGL_FRONT);
		if(selectionKey.size() != 0)
            cache->abcSceneManager.getScene(sceneKey)->drawOnly(cache->abcSceneState, selectionKey, cache->shaderColors, true);
        else
            cache->abcSceneManager.getScene(sceneKey)->draw(cache->abcSceneState, cache->shaderColors, true);
        glCullFace(MGL_BACK);
        if(selectionKey.size() != 0)
            cache->abcSceneManager.getScene(sceneKey)->drawOnly(cache->abcSceneState, selectionKey, cache->shaderColors, false);
        else
            cache->abcSceneManager.getScene(sceneKey)->draw(cache->abcSceneState, cache->shaderColors, false);
        gGLFT->glLightModeli(MGL_LIGHT_MODEL_TWO_SIDE, 1);
        glDisable(MGL_CULL_FACE);
    }
    else
    {
       if(selectionKey.size() != 0)
            cache->abcSceneManager.getScene(sceneKey)->drawOnly(cache->abcSceneState, selectionKey, cache->shaderColors, true);
        else
            cache->abcSceneManager.getScene(sceneKey)->draw(cache->abcSceneState, cache->shaderColors, true);
    }

    glDisable(MGL_POLYGON_OFFSET_FILL);
    glFrontFace(MGL_CCW);

    gGLFT->glPopMatrix();
}

void CAlembicHolderUI::drawBoundingBox( const MDrawRequest & request,
                                                          M3dView & view ) const
//
// Description:
//
//     Bounding box drawing routine
//
// Arguments:
//
//     request - request to be drawn
//     view    - view to draw into
//
{
        // Get the surface shape
        MPxSurfaceShape *shape = surfaceShape();
        if ( !shape )
                return;

        // Get the bounding box
        MBoundingBox box = shape->boundingBox();
    float w = (float) box.width();
    float h = (float) box.height();
    float d = (float) box.depth();

    {
        // Query current state so it can be restored
        //
        bool lightingWasOn = glIsEnabled( MGL_LIGHTING ) == MGL_TRUE;

        // Setup the OpenGL state as necessary
        //
        if ( lightingWasOn ) {
            glDisable( MGL_LIGHTING );
        }

        glEnable( MGL_LINE_STIPPLE );
        glLineStipple(1, LINE_STIPPLE_SHORTDASHED);

        // Below we just two sides and then connect
        // the edges together

        MPoint minVertex = box.min();

        request.color();

        // Draw first side
        glBegin( MGL_LINE_LOOP );
        MPoint vertex = minVertex;
        glVertex3f( (float)vertex[0],   (float)vertex[1],   (float)vertex[2] );
        glVertex3f( (float)vertex[0]+w, (float)vertex[1],   (float)vertex[2] );
        glVertex3f( (float)vertex[0]+w, (float)vertex[1]+h, (float)vertex[2] );
        glVertex3f( (float)vertex[0],   (float)vertex[1]+h, (float)vertex[2] );
        glVertex3f( (float)vertex[0],   (float)vertex[1],   (float)vertex[2] );
        glEnd();

        // Draw second side
        MPoint sideFactor(0,0,d);
        MPoint vertex2 = minVertex + sideFactor;
        glBegin( MGL_LINE_LOOP );
        glVertex3f( (float)vertex2[0],   (float)vertex2[1],   (float)vertex2[2] );
        glVertex3f( (float)vertex2[0]+w, (float)vertex2[1],   (float)vertex2[2] );
        glVertex3f( (float)vertex2[0]+w, (float)vertex2[1]+h, (float)vertex2[2] );
        glVertex3f( (float)vertex2[0],   (float)vertex2[1]+h, (float)vertex2[2] );
        glVertex3f( (float)vertex2[0],   (float)vertex2[1],   (float)vertex2[2] );
        glEnd();

        // Connect the edges together
        glBegin( MGL_LINES );
        glVertex3f( (float)vertex2[0],   (float)vertex2[1],   (float)vertex2[2] );
        glVertex3f( (float)vertex[0],   (float)vertex[1],   (float)vertex[2] );

        glVertex3f( (float)vertex2[0]+w,   (float)vertex2[1],   (float)vertex2[2] );
        glVertex3f( (float)vertex[0]+w,   (float)vertex[1],   (float)vertex[2] );

        glVertex3f( (float)vertex2[0]+w,   (float)vertex2[1]+h,   (float)vertex2[2] );
        glVertex3f( (float)vertex[0]+w,   (float)vertex[1]+h,   (float)vertex[2] );

        glVertex3f( (float)vertex2[0],   (float)vertex2[1]+h,   (float)vertex2[2] );
        glVertex3f( (float)vertex[0],   (float)vertex[1]+h,   (float)vertex[2] );
        glEnd();

        // Restore the state
        //
        if ( lightingWasOn ) {
            glEnable( MGL_LIGHTING );
        }

        glDisable( MGL_LINE_STIPPLE );
    }
}

void CAlembicHolderUI::getDrawRequestsWireFrame(MDrawRequest& request,
        const MDrawInfo& info) {

    request.setToken(kDrawWireframe);
    M3dView::DisplayStatus displayStatus = info.displayStatus();
    M3dView::ColorTable activeColorTable = M3dView::kActiveColors;
    M3dView::ColorTable dormantColorTable = M3dView::kDormantColors;
    switch (displayStatus) {
    case M3dView::kLead:
        request.setColor(LEAD_COLOR, activeColorTable);
        break;
    case M3dView::kActive:
        request.setColor(ACTIVE_COLOR, activeColorTable);
        break;
    case M3dView::kActiveAffected:
        request.setColor(ACTIVE_AFFECTED_COLOR, activeColorTable);
        break;
    case M3dView::kDormant:
        request.setColor(DORMANT_COLOR, dormantColorTable);
        break;
    case M3dView::kHilite:
        request.setColor(HILITE_COLOR, activeColorTable);
        break;
    default:
        break;
    }

}

void CAlembicHolderUI::getDrawRequestsBoundingBox(MDrawRequest& request,
        const MDrawInfo& info) {

    request.setToken(kDrawBoundingBox);
    M3dView::DisplayStatus displayStatus = info.displayStatus();
    M3dView::ColorTable activeColorTable = M3dView::kActiveColors;
    M3dView::ColorTable dormantColorTable = M3dView::kDormantColors;
    switch (displayStatus) {
    case M3dView::kLead:
        request.setColor(LEAD_COLOR, activeColorTable);
        break;
    case M3dView::kActive:
        request.setColor(ACTIVE_COLOR, activeColorTable);
        break;
    case M3dView::kActiveAffected:
        request.setColor(ACTIVE_AFFECTED_COLOR, activeColorTable);
        break;
    case M3dView::kDormant:
        request.setColor(DORMANT_COLOR, dormantColorTable);
        break;
    case M3dView::kHilite:
        request.setColor(HILITE_COLOR, activeColorTable);
        break;
    default:
        break;
    }

}


void CAlembicHolderUI::getDrawRequestsShaded(MDrawRequest& request,
        const MDrawInfo& info, MDrawRequestQueue& queue, MDrawData& data)
{
        // Need to get the material info
        //
        MDagPath path = info.multiPath();       // path to your dag object
        M3dView view = info.view();;            // view to draw to
        MMaterial material = MPxSurfaceShapeUI::material( path );
        M3dView::DisplayStatus displayStatus = info.displayStatus();

        // Evaluate the material and if necessary, the texture.
        //
        if ( ! material.evaluateMaterial( view, path ) ) {
                cerr << "Couldnt evaluate\n";
        }

        bool drawTexture = true;
        if ( drawTexture && material.materialIsTextured() ) {
                material.evaluateTexture( data );
        }

        request.setMaterial( material );

        bool materialTransparent = false;
        material.getHasTransparency( materialTransparent );
        if ( materialTransparent ) {
                request.setIsTransparent( true );
        }

        // create a draw request for wireframe on shaded if
        // necessary.
        //
        if ( (displayStatus == M3dView::kActive) ||
                 (displayStatus == M3dView::kLead) ||
                 (displayStatus == M3dView::kHilite) )
        {
                MDrawRequest wireRequest = info.getPrototype( *this );
                wireRequest.setDrawData( data );
                getDrawRequestsWireFrame( wireRequest, info );
                wireRequest.setToken( kDrawWireframeOnShaded );
                wireRequest.setDisplayStyle( M3dView::kWireFrame );
                queue.add( wireRequest );
        }
}

//
// Returns the point in world space corresponding to a given
// depth. The depth is specified as 0.0 for the near clipping plane and
// 1.0 for the far clipping plane.
//
MPoint CAlembicHolderUI::getPointAtDepth(
    MSelectInfo &selectInfo,
    double     depth) const
{
    MDagPath cameraPath;
    M3dView view = selectInfo.view();

    view.getCamera(cameraPath);
    MStatus status;
    MFnCamera camera(cameraPath, &status);

    // Ortho cam maps [0,1] to [near,far] linearly
    // persp cam has non linear z:
    //
    //        fp np
    // -------------------
    // 1. fp - d fp + d np
    //
    // Maps [0,1] -> [np,fp]. Then using linear mapping to get back to
    // [0,1] gives.
    //
    //       d np
    // ----------------  for linear mapped distance
    // fp - d fp + d np

    if (!camera.isOrtho())
    {
        double np = camera.nearClippingPlane();
        double fp = camera.farClippingPlane();

        depth *= np / (fp - depth * (fp - np));
    }

    MPoint     cursor;
    MVector rayVector;
    selectInfo.getLocalRay(cursor, rayVector);
    cursor = cursor * selectInfo.multiPath().inclusiveMatrix();
    short x,y;
    view.worldToView(cursor, x, y);

    MPoint res, neardb, fardb;
    view.viewToWorld(x,y, neardb, fardb);
    res = neardb + depth*(fardb-neardb);

    return res;
}


bool CAlembicHolderUI::select(MSelectInfo &selectInfo,
        MSelectionList &selectionList, MPointArray &worldSpaceSelectPts) const
{
    nozAlembicHolder* shapeNode = (nozAlembicHolder*) surfaceShape();
    CAlembicDatas* geom = shapeNode->alembicData();
    
    if(geom == NULL)
        return false;

    MSelectionMask mask("alembicHolder");
    if (!selectInfo.selectable(mask)){
        return false;
    }

    const bool boundingboxSelection =
        (M3dView::kBoundingBox == selectInfo.displayStyle() ||
         !selectInfo.singleSelection());
    
    
    if (boundingboxSelection)
    {
        // We hit the bounding box, so we want the object?
        MSelectionList item;
        item.add(selectInfo.selectPath());
        MPoint xformedPt;
        selectInfo.addSelection(item, xformedPt, selectionList,
                worldSpaceSelectPts, mask, false);
        return true;
    }


    std::string sceneKey = shapeNode->getSceneKey();

    GLfloat minZ;
    Select* selector;

    SimpleAbcViewer::ScenePtr _ptr = geom->abcSceneManager.getScene(sceneKey);
    size_t numTriangles = _ptr->getNumTriangles();
    const unsigned int bufferSize = (unsigned int)std::min(numTriangles,(size_t)100000);
        
    if (numTriangles < 1024)
        selector = new GLPickingSelect(selectInfo);
    else
        selector = new RasterSelect(selectInfo);

    selector->processTriangles(geom, sceneKey, numTriangles);
        
    selector->end();
    minZ = selector->minZ();
    delete selector;

  
    bool selected = (minZ <= 1.0f);
    if ( selected ) 
    {
        // Add the selected item to the selection list

        MSelectionList selectionItem;
        {
            MDagPath path = selectInfo.multiPath();
            MStatus lStatus = path.pop();
            while (lStatus == MStatus::kSuccess)
            {
                if (path.hasFn(MFn::kTransform))
                {
                    break;
                }
                else
                {
                    lStatus = path.pop();
                }
            }
            selectionItem.add(path);
        }        

        MPoint worldSpaceselectionPoint =
            getPointAtDepth(selectInfo, minZ);

        selectInfo.addSelection(
            selectionItem,
            worldSpaceselectionPoint,
            selectionList, worldSpaceSelectPts,
            mask, false );
    }

    return selected;

}
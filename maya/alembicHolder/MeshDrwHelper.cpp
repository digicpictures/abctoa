//-*****************************************************************************
//
// Copyright (c) 2009-2011,
//  Sony Pictures Imageworks, Inc. and
//  Industrial Light & Magic, a division of Lucasfilm Entertainment Company Ltd.
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// *       Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// *       Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
// *       Neither the name of Sony Pictures Imageworks, nor
// Industrial Light & Magic nor the names of their contributors may be used
// to endorse or promote products derived from this software without specific
// prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//-*****************************************************************************

#include "MeshDrwHelper.h"
#include "samplingUtils.h"

namespace AlembicHolder {

static MGLFunctionTable *gGLFT = NULL;


//-*****************************************************************************
MeshDrwHelper::MeshDrwHelper()
{
    makeInvalid();
    buffer.clear();
}

//-*****************************************************************************
MeshDrwHelper::~MeshDrwHelper()
{
    makeInvalid();
}

//-*****************************************************************************
void MeshDrwHelper::update( P3fArraySamplePtr iP,
                            P3fArraySamplePtr iPCeil,
                            N3fArraySamplePtr iN,
                            Int32ArraySamplePtr iIndices,
                            Int32ArraySamplePtr iCounts,
                            Abc::Box3d iBounds, 
                            double alpha )
{

   // Before doing a ton, just have a quick look.

    if ( m_meshP && iP &&
         ( m_meshP->size() == iP->size() ) &&
         m_meshIndices &&
         ( m_meshIndices == iIndices ) &&
         m_meshCounts &&
         ( m_meshCounts == iCounts ) )
    {
        if ( m_meshP == iP )
        {
            updateNormals( iN );
        }
        else
        {
            update( iP, iPCeil, iN, iBounds, alpha );
        }
        return;
    }

    buffer.clear();
    // Okay, if we're here, the indices are not equal or the counts
    // are not equal or the P-array size changed.
    // So we can clobber those three, but leave N alone for now.
    m_triangles.clear ();
    m_meshP = iP;
    m_meshIndices = iIndices;
    m_meshCounts = iCounts;



    // Check stuff.
    if ( !m_meshP ||
         !m_meshIndices ||
         !m_meshCounts )
    {
        std::cerr << "Mesh update quitting because no input data"
                  << std::endl;
        makeInvalid();
        return;
    }

    // Get the number of each thing.
    size_t numFaces = m_meshCounts->size();
    size_t numIndices = m_meshIndices->size();
    size_t numPoints = m_meshP->size();
    if ( numFaces < 1 ||
         numIndices < 1 ||
         numPoints < 1 )
    {
        // Invalid.
        std::cerr << "Mesh update quitting because bad arrays"
                  << ", numFaces = " << numFaces
                  << ", numIndices = " << numIndices
                  << ", numPoints = " << numPoints
                  << std::endl;
        makeInvalid();
        return;
    }

    // Make triangles.

    std::vector<MGLuint> vidx;

    size_t faceIndexBegin = 0;
    size_t faceIndexEnd = 0;
    for ( size_t face = 0; face < numFaces; ++face )
    {
        faceIndexBegin = faceIndexEnd;
        size_t count = (*m_meshCounts)[face];


        faceIndexEnd = faceIndexBegin + count;

        // Check this face is valid
        if ( faceIndexEnd > numIndices ||
             faceIndexEnd < faceIndexBegin )
        {
            std::cerr << "Mesh update quitting on face: "
                      << face
                      << " because of wonky numbers"
                      << ", faceIndexBegin = " << faceIndexBegin
                      << ", faceIndexEnd = " << faceIndexEnd
                      << ", numIndices = " << numIndices
                      << ", count = " << count
                      << std::endl;

            // Just get out, make no more triangles.
            break;
        }

        // Checking indices are valid.
        bool goodFace = true;
        for ( size_t fidx = faceIndexBegin;
              fidx < faceIndexEnd; ++fidx )
        {
            if ( ( size_t ) ( (*m_meshIndices)[fidx] ) >= numPoints )
            {
                std::cout << "Mesh update quitting on face: "
                          << face
                          << " because of bad indices"
                          << ", indexIndex = " << fidx
                          << ", vertexIndex = " << (*m_meshIndices)[fidx]
                          << ", numPoints = " << numPoints
                          << std::endl;
                goodFace = false;
                break;
            }
        }

        // Make triangles to fill this face.
        if ( goodFace && count > 2 )
        {
            vidx.push_back(( unsigned int )(*m_meshIndices)[faceIndexBegin+0]);
            vidx.push_back(( unsigned int )(*m_meshIndices)[faceIndexBegin+1]);
            vidx.push_back(( unsigned int )(*m_meshIndices)[faceIndexBegin+2]);

            m_triangles.push_back(
                Tri( ( unsigned int )(*m_meshIndices)[faceIndexBegin+0],
                     ( unsigned int )(*m_meshIndices)[faceIndexBegin+1],
                     ( unsigned int )(*m_meshIndices)[faceIndexBegin+2] ) );
            for ( size_t c = 3; c < count; ++c )
            {
                vidx.push_back(( unsigned int )(*m_meshIndices)[faceIndexBegin+0]);
                vidx.push_back(( unsigned int )(*m_meshIndices)[faceIndexBegin+c-1]);
                vidx.push_back(( unsigned int )(*m_meshIndices)[faceIndexBegin+c]);

                m_triangles.push_back(
                    Tri( ( unsigned int )(*m_meshIndices)[faceIndexBegin+0],
                         ( unsigned int )(*m_meshIndices)[faceIndexBegin+c-1],
                         ( unsigned int )(*m_meshIndices)[faceIndexBegin+c]
                         ) );
            }
        }
    }



    std::vector<MGLfloat> v;

    //size_t numPoints = m_meshP->size();

    
    {
        for ( size_t p = 0; p < numPoints; ++p )
        {
            const V3f &P = (*m_meshP)[p];
            if (alpha == 0 || iPCeil == NULL) 
            {
                v.push_back(P.x);
                v.push_back(P.y);
                v.push_back(P.z);
            }
            else
            {
                v.push_back(simpleLerp<float>(alpha, P.x, (*iPCeil)[p].x));
                v.push_back(simpleLerp<float>(alpha, P.y, (*iPCeil)[p].y));
                v.push_back(simpleLerp<float>(alpha, P.z, (*iPCeil)[p].z));
            }
        }
    }

    buffer.genVertexBuffer(v);
    buffer.genIndexBuffer(vidx, MGL_TRIANGLES);

    m_valid = true;

    // And now update just the P and N, which will update bounds
    // and calculate new normals if necessary.


    if ( iBounds.isEmpty() )
    {
        computeBounds();
    }
    else
    {
        m_bounds = iBounds;
    }
    
    updateNormals( iN );

    pushNormals();


    // And that's it.
}

//-*****************************************************************************
void MeshDrwHelper::update( P3fArraySamplePtr iP,
                            P3fArraySamplePtr iPCeil,
                            N3fArraySamplePtr iN,
                            Abc::Box3d iBounds, 
                            double alpha )
{
    // Check validity.
    if ( !m_valid || !iP || !m_meshP ||
         ( iP->size() != m_meshP->size() ) )
    {
        makeInvalid();
        buffer.clear();
        return;
    }

    // Set meshP
    m_meshP = iP;

    if ( iBounds.isEmpty() )
    {
        computeBounds();
    }
    else
    {
        m_bounds = iBounds;
    }

    updateNormals( iN );
}

void MeshDrwHelper::pushNormals()
{
    const V3f *normals = NULL;
    if ( m_meshN  && ( m_meshN->size() == m_meshP->size() ) )
        normals = m_meshN->get(); 
    else if ( m_customN.size() == m_meshP->size() )
        normals = &(m_customN.front());

    if(normals != NULL)
    {
        const auto N = m_meshP->size();
        std::vector<MGLfloat> v;
        v.reserve(3 * N);
        std::vector<MGLfloat> vflipped;
        vflipped.reserve(3 * N);
        for ( size_t p = 0; p < N; ++p )
        {
            V3f normal = normals[p];

            v.push_back(normal.x);
            v.push_back(normal.y);
            v.push_back(normal.z);

            vflipped.push_back(-normal.x);
            vflipped.push_back(-normal.y);
            vflipped.push_back(-normal.z);

        }

        buffer.genNormalBuffer(v);
        buffer.genNormalBuffer(vflipped, true);
    }
}

//-*****************************************************************************
void MeshDrwHelper::updateNormals( N3fArraySamplePtr iN )
{

    if ( !m_valid || !m_meshP )
    {
        makeInvalid();
        buffer.clear();
        return;
    }

    // Now see if we need to calculate normals.
    if ( ( m_meshN && iN == m_meshN ) || (isConstant() && m_customN.size() > 0 ))
    {
        return;
    }

    size_t numPoints = m_meshP->size();
    m_meshN = iN;
    m_customN.clear();


    // Right now we only handle "vertex varying" normals,
    // which have the same cardinality as the points
    if ( !m_meshN || m_meshN->size() != numPoints )
    {
        
        m_meshN.reset();
        m_customN.resize( numPoints );

        std::fill( m_customN.begin(), m_customN.end(), V3f( 0.0f ) );

        // compute the face normals
        for ( size_t tidx = 0;  tidx < m_triangles.size(); ++tidx )
        {
            const Tri &tri = m_triangles[tidx];
            const V3f &A = (*m_meshP)[tri[0]];
            const V3f &B = (*m_meshP)[tri[1]];
            const V3f &C = (*m_meshP)[tri[2]];
            V3f AB = B - A;
            V3f AC = C - A;
            V3f wN = AB.cross( AC );
            m_customN[tri[0]] += wN;
            m_customN[tri[1]] += wN;
            m_customN[tri[2]] += wN;
        }
        // Normalize normals.
        for ( size_t nidx = 0; nidx < numPoints; ++nidx )
        {
            m_customN[nidx].normalize();
        }
    }

    

}
//-*****************************************************************************

void MeshDrwHelper::updateArbs(Alembic::Abc::ICompoundProperty & iParent,
                 Int32ArraySamplePtr iIndices,
                 Int32ArraySamplePtr iCounts )
{

}
//-*****************************************************************************
void MeshDrwHelper::draw( const DrawContext & iCtx) const
{

    // Bail if invalid.
    if ( !m_valid || m_triangles.size() < 1 || !m_meshP )
    {
        return;
    }

    buffer.render(iCtx.isNormalFlipped());

}

//-*****************************************************************************
void MeshDrwHelper::makeInvalid()
{
    m_meshP.reset();
    m_meshN.reset();
    m_meshIndices.reset();
    m_meshCounts.reset();
    m_customN.clear();
    //m_colors.clear();
    m_valid = false;
    m_bounds.makeEmpty();
    m_triangles.clear();
}

int MeshDrwHelper::getNumTriangles() const
{
    return m_triangles.size();
}

//-*****************************************************************************
void MeshDrwHelper::computeBounds()
{
    m_bounds.makeEmpty();
    if ( m_meshP )
    {
        size_t numPoints = m_meshP->size();
        for ( size_t p = 0; p < numPoints; ++p )
        {
            const V3f &P = (*m_meshP)[p];
            m_bounds.extendBy( V3d( P.x, P.y, P.z ) );
        }
    }
}

} // End namespace AlembicHolder

#include "assimp_model.h"
#include "shader.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <vector>
#include <cstring>
#include <filesystem>

// Interleaved layout: pos(3), normal(3), uv(2), tangent(4)
static void make_vao(const std::vector<float>& interleaved, const std::vector<uint32_t>& indices, AMeshPrimitive& out) {
    glGenVertexArrays(1, &out.vao);
    glBindVertexArray(out.vao);
    glGenBuffers(1, &out.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, out.vbo);
    glBufferData(GL_ARRAY_BUFFER, interleaved.size()*sizeof(float), interleaved.data(), GL_STATIC_DRAW);
    glGenBuffers(1, &out.ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, out.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size()*sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);
    out.indexCount = (GLsizei)indices.size();
    out.indexType = GL_UNSIGNED_INT;
    const GLsizei stride = 12 * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,stride,(void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,stride,(void*)(3*sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,stride,(void*)(6*sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3,4,GL_FLOAT,GL_FALSE,stride,(void*)(8*sizeof(float)));
    glBindVertexArray(0);
}

void AssimpModel::clear(){
    for (auto& m: meshes_) {
        if (m.ebo) glDeleteBuffers(1,&m.ebo);
        if (m.vbo) glDeleteBuffers(1,&m.vbo);
        if (m.vao) glDeleteVertexArrays(1,&m.vao);
    }
    meshes_.clear();
    for (auto& mat : materials_) {
        if (mat.baseColorTex) glDeleteTextures(1, &mat.baseColorTex);
        if (mat.ormTex) glDeleteTextures(1, &mat.ormTex);
        if (mat.normalTex) glDeleteTextures(1, &mat.normalTex);
    }
    materials_.clear();
}

bool AssimpModel::load(const std::string& path) {
    clear();
    Assimp::Importer importer;
    unsigned flags = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_GenNormals |
                     aiProcess_CalcTangentSpace | aiProcess_FlipUVs | aiProcess_GenUVCoords | aiProcess_ImproveCacheLocality |
                     aiProcess_SortByPType | aiProcess_OptimizeMeshes | aiProcess_OptimizeGraph | aiProcess_PreTransformVertices;
    const aiScene* scene = importer.ReadFile(path, flags);
    if (!scene || !scene->mRootNode) return false;
    std::filesystem::path p(path);
    baseDir_ = p.has_parent_path() ? p.parent_path().string() : std::string(".");

    // Materials
    materials_.resize(scene->mNumMaterials);
    for (unsigned mi=0; mi<scene->mNumMaterials; ++mi) {
        const aiMaterial* aim = scene->mMaterials[mi];
        auto& dst = materials_[mi];
        aiColor4D col;
        if (AI_SUCCESS == aim->Get(AI_MATKEY_BASE_COLOR, col)) {
            dst.baseColorFactor = {col.r, col.g, col.b, col.a};
        }
        aim->Get(AI_MATKEY_METALLIC_FACTOR, dst.metallicFactor);
        aim->Get(AI_MATKEY_ROUGHNESS_FACTOR, dst.roughnessFactor);

        // Base color texture
        if (aim->GetTextureCount(aiTextureType_BASE_COLOR) > 0) {
            aiString t; aim->GetTexture(aiTextureType_BASE_COLOR, 0, &t);
            const aiTexture* emb = scene->GetEmbeddedTexture(t.C_Str());
            if (emb) dst.baseColorTex = loadTextureFromAssimp(emb, true);
            else dst.baseColorTex = loadTextureFromFile((std::filesystem::path(baseDir_) / t.C_Str()).string(), true);
            dst.hasBaseColor = dst.baseColorTex != 0;
        }
        // ORM (occlusion-roughness-metallic) often exported as UNKNOWN for glTF2 in Assimp
        if (aim->GetTextureCount(aiTextureType_UNKNOWN) > 0) {
            aiString t; aim->GetTexture(aiTextureType_UNKNOWN, 0, &t);
            const aiTexture* emb = scene->GetEmbeddedTexture(t.C_Str());
            if (emb) dst.ormTex = loadTextureFromAssimp(emb, false);
            else dst.ormTex = loadTextureFromFile((std::filesystem::path(baseDir_) / t.C_Str()).string(), false);
            dst.hasORM = dst.ormTex != 0;
        }
        // Normal map
        if (aim->GetTextureCount(aiTextureType_NORMALS) > 0) {
            aiString t; aim->GetTexture(aiTextureType_NORMALS, 0, &t);
            const aiTexture* emb = scene->GetEmbeddedTexture(t.C_Str());
            if (emb) dst.normalTex = loadTextureFromAssimp(emb, false);
            else dst.normalTex = loadTextureFromFile((std::filesystem::path(baseDir_) / t.C_Str()).string(), false);
            dst.hasNormal = dst.normalTex != 0;
        }
        // Try to infer glass-like dispersion via material flags (no standard in Assimp): if baseColor alpha < 1 and metallic ~0
        if (dst.baseColorFactor.a < 0.99f && dst.metallicFactor < 0.1f) {
            dst.dispersion = true; dst.ior = 1.5f; dst.abbe = 50.0f;
        }
    }

    // Iterate meshes already pre-transformed to world due to PreTransformVertices
    meshes_.reserve(scene->mNumMeshes);
    for (unsigned i=0;i<scene->mNumMeshes;++i){
        const aiMesh* mesh = scene->mMeshes[i];
        if (!mesh->HasPositions()) continue;
        std::vector<float> interleaved;
        interleaved.resize(mesh->mNumVertices * 12);
        for (unsigned v=0; v<mesh->mNumVertices; ++v){
            const aiVector3D& p = mesh->mVertices[v];
            const aiVector3D n = mesh->HasNormals()? mesh->mNormals[v] : aiVector3D(0,0,1);
            aiVector3D uv = mesh->HasTextureCoords(0)? mesh->mTextureCoords[0][v] : aiVector3D(0,0,0);
            aiVector3D tan = mesh->HasTangentsAndBitangents()? mesh->mTangents[v] : aiVector3D(1,0,0);
            float tanW = 1.0f; // sign; Assimp doesn't directly expose handedness; assume +1
            interleaved[v*12+0]=p.x; interleaved[v*12+1]=p.y; interleaved[v*12+2]=p.z;
            interleaved[v*12+3]=n.x; interleaved[v*12+4]=n.y; interleaved[v*12+5]=n.z;
            interleaved[v*12+6]=uv.x; interleaved[v*12+7]=uv.y;
            interleaved[v*12+8]=tan.x; interleaved[v*12+9]=tan.y; interleaved[v*12+10]=tan.z; interleaved[v*12+11]=tanW;
        }
        std::vector<uint32_t> indices;
        indices.reserve(mesh->mNumFaces*3);
        for (unsigned f=0; f<mesh->mNumFaces; ++f) {
            const aiFace& face = mesh->mFaces[f];
            if (face.mNumIndices==3) {
                indices.push_back(face.mIndices[0]);
                indices.push_back(face.mIndices[1]);
                indices.push_back(face.mIndices[2]);
            }
        }
        AMeshPrimitive prim{}; make_vao(interleaved, indices, prim);
        prim.materialIndex = (int)mesh->mMaterialIndex;
        meshes_.push_back(prim);
    }
    return !meshes_.empty();
}

void AssimpModel::draw(const ShaderProgram& shader) const {
    for (const auto& m: meshes_) {
        if (m.materialIndex >= 0 && m.materialIndex < (int)materials_.size()) {
            const auto& mat = materials_[m.materialIndex];
            // bind samplers once per draw
            shader.set1i("uBaseColorTex", 0);
            shader.set1i("uORMTex", 1);
            shader.set1i("uNormalTex", 2);
            if (mat.baseColorTex) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, mat.baseColorTex); }
            if (mat.ormTex)       { glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, mat.ormTex); }
            if (mat.normalTex)    { glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, mat.normalTex); }
            shader.set1f("uMetallic", mat.metallicFactor);
            shader.set1f("uRoughness", mat.roughnessFactor);
            shader.set1i("uHasORM", mat.hasORM ? 1 : 0);
            shader.set1i("uHasNormal", mat.hasNormal ? 1 : 0);
            shader.set1i("uDispersionEnabled", mat.dispersion ? 1 : 0);
            shader.set1f("uAbbeNumber", mat.abbe);
            shader.set1f("uIOR", mat.ior);
        }
        glBindVertexArray(m.vao);
        glDrawElements(GL_TRIANGLES, m.indexCount, m.indexType, 0);
    }
    glBindVertexArray(0);
}

GLuint AssimpModel::loadTextureFromAssimp(const aiTexture* tex, bool srgb) const {
    if (!tex) return 0;
    int w=0,h=0,c=0; unsigned char* data = nullptr;
    if (tex->mHeight == 0) {
        data = stbi_load_from_memory(reinterpret_cast<const unsigned char*>(tex->pcData), (int)tex->mWidth, &w,&h,&c, 0);
    } else {
        data = stbi_load_from_memory(reinterpret_cast<const unsigned char*>(tex->pcData), (int)(tex->mWidth * tex->mHeight), &w,&h,&c, 0);
    }
    if (!data) return 0;
    GLenum internal = (c==4) ? (srgb?GL_SRGB8_ALPHA8:GL_RGBA8) : (srgb?GL_SRGB8:GL_RGB8);
    GLenum format = (c==4)?GL_RGBA:GL_RGB;
    GLuint id=0; glGenTextures(1,&id);
    glBindTexture(GL_TEXTURE_2D,id);
    glTexImage2D(GL_TEXTURE_2D,0,internal,w,h,0,format,GL_UNSIGNED_BYTE,data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    stbi_image_free(data);
    return id;
}

GLuint AssimpModel::loadTextureFromFile(const std::string& path, bool srgb) const {
    int w=0,h=0,c=0; unsigned char* data = stbi_load(path.c_str(), &w,&h,&c, 0);
    if (!data) return 0;
    GLenum internal = (c==4) ? (srgb?GL_SRGB8_ALPHA8:GL_RGBA8) : (srgb?GL_SRGB8:GL_RGB8);
    GLenum format = (c==4)?GL_RGBA:GL_RGB;
    GLuint id=0; glGenTextures(1,&id);
    glBindTexture(GL_TEXTURE_2D,id);
    glTexImage2D(GL_TEXTURE_2D,0,internal,w,h,0,format,GL_UNSIGNED_BYTE,data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    stbi_image_free(data);
    return id;
}

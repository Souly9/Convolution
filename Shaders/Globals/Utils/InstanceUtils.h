
mat4 FetchWorldMatrix(uint glInstanceIdx) {
   uint instanceIdx = perObjectDataSSBO.transformDataIdx[glInstanceIdx];
   InstanceData iData = globalInstanceDataSSBO.instances[instanceIdx];
   uint transformIdx = GetTransformIdx(iData);
   mat4 worldMat = globalTransformSSBO.modelMatrices[transformIdx];
   return worldMat;
}
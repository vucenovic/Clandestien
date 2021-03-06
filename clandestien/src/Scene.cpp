#include "Scene.h"

bool Scene::AddObject(std::unique_ptr<GameObject> & obj)
{
	if (gameObjects.find(obj->name) == gameObjects.end()) {
		auto & shaderGroup = renderGroups[obj->material->shader];
		std::unordered_map<Mesh*, std::vector<GameObject*>> & meshGroups = shaderGroup[obj->material];
		std::vector<GameObject*> & objects = meshGroups[obj->mesh];
		objects.push_back(obj.get());
		gameObjects[obj->name] = std::move(obj);
		return true;
	}
	return false; //GameObject already exists
}

GameObject * Scene::GetObject(const std::string & name)
{
	if (gameObjects.find(name) == gameObjects.end()) {
		return nullptr;
	}
	return gameObjects[name].get();
}

void Scene::RemoveObject(const std::string & name)
{
	auto & obj = gameObjects[name];
	auto & shaderGroup = renderGroups[obj->material->shader];
	std::unordered_map<Mesh*, std::vector<GameObject*>> & meshGroups = shaderGroup[obj->material];
	std::vector<GameObject*> & objects = meshGroups[obj->mesh];
	for (int i = 0; i < objects.size(); i++) {
		if (objects[i] == obj.get()) {
			objects.erase(objects.begin() + i);
		}
	}
	if (objects.size() == 0) meshGroups.erase(obj->mesh);
	if (meshGroups.size() == 0) shaderGroup.erase(obj->material);
	if (shaderGroup.size() == 0) renderGroups.erase(obj->material->shader);
	gameObjects.erase(name);
}

Scene::Scene(LightManager& lm) : lm(lm)
{
}

void Scene::DrawOpaqueObjects(const Material & material)
{
	material.Use();
	GLuint modelMatrixLocation = material.shader->GetUniformLocation("modelMatrix");
	GLuint normalMatrixLocation = material.shader->GetUniformLocation("modelNormalMatrix");

	for (std::pair<ShaderProgram*, std::unordered_map<Material*, std::unordered_map<Mesh*, std::vector<GameObject*>>>> shaderGroup : renderGroups)
	{
		for (std::pair<Material*, std::unordered_map<Mesh*, std::vector<GameObject*>>> meshGroup : shaderGroup.second) {
			for (std::pair<Mesh*, std::vector<GameObject*>> GameObjects : meshGroup.second)
			{
				GameObjects.first->Bind();
				for (GameObject* gameObject : GameObjects.second)
				{
					glUniformMatrix4fv(modelMatrixLocation, 1, GL_FALSE, glm::value_ptr(gameObject->GetTransform().ToMatrix()));
					glUniformMatrix4fv(normalMatrixLocation, 1, GL_FALSE, glm::value_ptr(gameObject->GetTransform().ToNormalMatrix()));
					GameObjects.first->Draw();
				}
			}
		}
	}
}

void Scene::DrawOpaqueObjects()
{
	for (auto shaderGroup : renderGroups)
	{
		GLuint modelMatrixLocation = shaderGroup.first->GetUniformLocation("modelMatrix");
		GLuint normalMatrixLocation = shaderGroup.first->GetUniformLocation("modelNormalMatrix");

		for (auto meshGroup : shaderGroup.second) {
			meshGroup.first->Use();

			for (auto GameObjects : meshGroup.second)
			{
				GameObjects.first->Bind();
				for (GameObject* gameObject : GameObjects.second)
				{
					glUniformMatrix4fv(modelMatrixLocation, 1, GL_FALSE, glm::value_ptr(gameObject->GetTransform().ToMatrix()));
					glUniformMatrix4fv(normalMatrixLocation, 1, GL_FALSE, glm::value_ptr(gameObject->GetTransform().ToNormalMatrix()));
					GameObjects.first->Draw();
				}
			}
		}
	}
}

void Scene::DrawTransparentObjects()
{
	
}

void Scene::RenderPortal(const Portal * portal)
{
	glEnable(GL_STENCIL_TEST);
	glClear(GL_STENCIL_BUFFER_BIT);

	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	glStencilFunc(GL_ALWAYS, 1, 0xFF);
	glStencilMask(0xFF);
	glColorMask(false, false, false, false);

	activeCamera->UseCamera(*viewDataBuffer);
	portalHoldoutShader->UseProgram();
	GLuint modelMatrixLocation = portalHoldoutShader->GetUniformLocation("modelMatrix");

	glUniformMatrix4fv(modelMatrixLocation, 1, GL_FALSE, glm::value_ptr(portal->transform.ToMatrix()));
	portal->portalMesh->BindAndDraw();

	//clear depth inside the portal
	glStencilFunc(GL_EQUAL, 1, 0xFF);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	glStencilMask(0x00);
	glDepthFunc(GL_ALWAYS);
	glColorMask(true, true, true, true);

	depthResetSS->UseProgram();
	glBindVertexArray(SSrectVAOId);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	
	glDepthFunc(GL_LESS);

	//SetViewParameters
	glm::mat4 view = glm::inverse(portal->getOffsetMatrix() * activeCamera->GetTransform().ToMatrix());

	glm::vec3 dir = -portal->targetTransform.GetForward();
	glm::vec4 C = glm::vec4(dir, glm::dot(-dir, portal->targetTransform.GetPosition()));
	C = glm::transpose(glm::inverse(view)) * C;

	glm::mat4 projection = activeCamera->GetObliqueProjection(C);
	Camera::SetViewParameters(*viewDataBuffer, view, projection);

	//draw scene from the portal
	DrawOpaqueObjects();
	DrawTransparentObjects();

	//redraw original portal depth (to prevent stuff from being drawn inside the portal after the portal is finished drawing)
	glColorMask(false, false, false, false);
	glDepthFunc(GL_ALWAYS);

	activeCamera->UseCamera(*viewDataBuffer);
	portalHoldoutShader->UseProgram();
	portal->portalMesh->BindAndDraw();

	glColorMask(true, true, true, true);
	glDepthFunc(GL_LESS);

	//clear stencil
	glStencilMask(0xFF);
	glClear(GL_STENCIL_BUFFER_BIT);
	glDisable(GL_STENCIL_TEST);
}

void Scene::DrawScene(bool drawPortals)
{
	//Draw WorldPortals between Opaque phase and transparent phase
	//use stencil buffer for masking drawing and an oblique projection

	//render portal to stencil buffer
	//render screen quad with only stencil testing and write depth to be the far plane (to reset depth buffer in the portal)
	//rerender scene from other perspective with stencil and depth testing
	//?
	//profit

	activeCamera->UseCamera(*viewDataBuffer);
	DrawOpaqueObjects();
	if (drawPortals) {
		for(Portal portal : renderPortals){
			RenderPortal(&portal);
		}
	}
	activeCamera->UseCamera(*viewDataBuffer);
	DrawTransparentObjects();
}

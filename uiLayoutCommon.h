#pragma once
namespace UILayoutCommon {
	// "SceneN/12" : sub info, placed just above "Jump to Scene" (bottom right)
	// (the scene title is already shown at the top left, and the top right is used by the GPU stats)
	constexpr int currentScenePositionX = 650;
	constexpr int currentScenePositionY = 550;
	constexpr float currentSceneScale = 0.7f;

	// index of the last scene (used for the "/12" part). Update this when adding a scene
	constexpr int lastSceneIndex = 12;

	constexpr int howToUsePositionX = 650;
	constexpr int howToUsePositionY = 580;
	constexpr int howToUseSpaceY = 30;
}
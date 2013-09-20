/*
    This file is part of Soupe Au Caillou.

    @author Soupe au Caillou - Jordane Pelloux-Prayer
    @author Soupe au Caillou - Gautier Pelloux-Prayer
    @author Soupe au Caillou - Pierre-Eric Pelloux-Prayer

    Soupe Au Caillou is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 3.

    Soupe Au Caillou is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Soupe Au Caillou.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "GameCenterAPIHelper.h"

#include "base/EntityManager.h"

#include <systems/RenderingSystem.h>
#include <systems/ButtonSystem.h>

void GameCenterAPIHelper::init(GameCenterAPI * gameCenterAPI) {
    _gameCenterAPI = gameCenterAPI;

    bUIdisplayed = false;

    //creat needed entities
    signButton = theEntityManager.CreateEntity("btn_gg_sign",
        EntityType::Volatile, theEntityManager.entityTemplateLibrary.load("googleplay/signinout_button"));

    achievementsButton = theEntityManager.CreateEntity("btn_gg_achievements",
        EntityType::Volatile, theEntityManager.entityTemplateLibrary.load("googleplay/achievements_button"));

    leaderboardsButton = theEntityManager.CreateEntity("btn_gg_leaderboards",
        EntityType::Volatile, theEntityManager.entityTemplateLibrary.load("googleplay/leaderboards_button"));
}


void GameCenterAPIHelper::displayFeatures(bool display) {
    RENDERING(achievementsButton)->show = RENDERING(leaderboardsButton)->show =
    BUTTON(achievementsButton)->enabled = BUTTON(leaderboardsButton)->enabled = display;
}

void GameCenterAPIHelper::displayUI() {
    LOGF_IF (! _gameCenterAPI, "Asked to display GameCenter UI but it was not correctly initialized" );

    bUIdisplayed = true;
    RENDERING(signButton)->show = BUTTON(signButton)->enabled = true;

    //only display other buttons if we are connected
    displayFeatures(_gameCenterAPI->isConnected());
}

void GameCenterAPIHelper::hideUI() {
    LOGF_IF (! _gameCenterAPI, "Asked to display GameCenter UI but it was not correctly initialized" );

    bUIdisplayed = false;
    RENDERING(signButton)->show = BUTTON(signButton)->enabled = false;

    displayFeatures(false);
}  

void GameCenterAPIHelper::updateUI() {
    LOGF_IF (! _gameCenterAPI, "Asked to display GameCenter UI but it was not correctly initialized" );

    bool isConnected = _gameCenterAPI->isConnected();

    displayFeatures(bUIdisplayed & isConnected);
    
    if (isConnected) {
        RENDERING(signButton)->texture = theRenderingSystem.loadTextureFile("gg_signout");
    } else {
        RENDERING(signButton)->texture = theRenderingSystem.loadTextureFile("gg_signin");
    }

    if (BUTTON(signButton)->clicked) {
        isConnected ? _gameCenterAPI->disconnect() : _gameCenterAPI->connectOrRegister();
    } else if (BUTTON(achievementsButton)->clicked) {
        _gameCenterAPI->openAchievement();
    } else if (BUTTON(leaderboardsButton)->clicked) {
        _gameCenterAPI->openLeaderboards();
    }


}

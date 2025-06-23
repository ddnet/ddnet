// This file can be included several times.

#ifndef COMPONENT
#error "The component macros must be defined"
#define COMPONENT(x) ;
#define SUBCOMPONENT(x, y) ;
#define COMPONENTINTERFACE(x) ;
#endif

COMPONENT(LocalServer)
COMPONENT(Flow)
COMPONENT(Skins)
COMPONENT(Skins7)
COMPONENT(CountryFlags)
COMPONENT(MapImages)
COMPONENT(Effects) // doesn't render anything, just updates effects
COMPONENT(Binds)
SUBCOMPONENT(Binds, m_SpecialBinds)
COMPONENT(Controls)
COMPONENT(Camera)
COMPONENT(Sounds)
COMPONENT(Voting)
COMPONENT(Particles) // doesn't render anything, just updates all the particles
COMPONENT(RaceDemo)
COMPONENT(MapSounds)
COMPONENT(Background) // render instead of m_MapLayersBackground when g_Config.m_ClOverlayEntities == 100
COMPONENT(MapLayersBackground) // first to render
SUBCOMPONENT(Particles, m_RenderTrail)
SUBCOMPONENT(Particles, m_RenderTrailExtra)
COMPONENT(Items)
COMPONENT(Ghost)
COMPONENT(Players)
COMPONENT(MapLayersForeground)
SUBCOMPONENT(Particles, m_RenderExplosions)
COMPONENT(NamePlates)
SUBCOMPONENT(Particles, m_RenderExtra)
SUBCOMPONENT(Particles, m_RenderGeneral)
COMPONENT(FreezeBars)
COMPONENT(DamageInd)
COMPONENT(Hud)
COMPONENT(Spectator)
COMPONENT(Emoticon)
COMPONENT(InfoMessages)
COMPONENT(Chat)
COMPONENT(Broadcast)
COMPONENT(DebugHud)
COMPONENT(TouchControls)
COMPONENT(Scoreboard)
COMPONENT(Statboard)
COMPONENT(Motd)
COMPONENT(Menus)
COMPONENT(Tooltips)
SUBCOMPONENT(Menus, m_Binder)
COMPONENT(GameConsole)
COMPONENT(MenuBackground)

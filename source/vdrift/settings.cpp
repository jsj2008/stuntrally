#include "pch.h"
#include "settings.h"
#include <stdio.h>


void SETTINGS::Load(std::string sfile)
{
	CONFIGFILE c;  c.Load(sfile);
	Serialize(false, c);
}
void SETTINGS::Save(std::string sfile)
{
	CONFIGFILE c;  c.Load(sfile);  version = SET_VER;
	Serialize(true, c);  c.Write();
}

void SETTINGS::Serialize(bool w, CONFIGFILE & c)
{
	c.bFltFull = false;
	for (int i=0; i < 4; ++i)
	{
		char ss[64];  sprintf(ss, "car%d.", i+1);   std::string s = ss;
		Param(c,w, s+"car", car[i]);				Param(c,w, s+"clr_hue", car_hue[i]);
		Param(c,w, s+"clr_sat", car_sat[i]);		Param(c,w, s+"clr_val", car_val[i]);
		Param(c,w, s+"camera", cam_view[i]);
	}
	Param(c,w, "car1.autotrans", autoshift);	//todo: this for all 4 cars..
	Param(c,w, "car1.autorear", autorear);		Param(c,w, "car1.autorear_inv", rear_inv);
	Param(c,w, "car1.abs", abs);				Param(c,w, "car1.tcs", tcs);

	//  game
	Param(c,w, "game.boost_type", boost_type);			Param(c,w, "game.flip_type", flip_type);
	Param(c,w, "game.boost_power", boost_power);
	Param(c,w, "game.collis_cars", collis_cars);		Param(c,w, "game.collis_veget", collis_veget);
	Param(c,w, "game.track", track);					Param(c,w, "game.track_user", track_user);
	Param(c,w, "game.trk_reverse", trackreverse);
	Param(c,w, "game.local_players", local_players);	Param(c,w, "game.split_vertically", split_vertically);
	Param(c,w, "game.num_laps", num_laps);

	//  hud
	Param(c,w, "hud_show.fps", show_fps);				Param(c,w, "hud_show.mph", show_mph);
	Param(c,w, "hud_show.gauges", show_gauges);			Param(c,w, "hud_show.show_digits", show_digits);
	Param(c,w, "hud_show.trackmap", trackmap);
	Param(c,w, "hud_show.caminfo", show_cam);			Param(c,w, "hud_show.times", show_times);
	Param(c,w, "hud_show.car_dbgtxt", car_dbgtxt);		Param(c,w, "hud_show.show_cardbg", car_dbgbars);
	Param(c,w, "hud_show.tracks_view", tracks_view);
	Param(c,w, "hud_show.tracks_sort", tracks_sort);	Param(c,w, "hud_show.tracks_sortup", tracks_sortup);
	Param(c,w, "hud_show.check_arrow", check_arrow);	Param(c,w, "hud_show.opponents", show_opponents);

	Param(c,w, "hud_size.gauges", size_gauges);			Param(c,w, "hud_size.minimap", size_minimap);
	Param(c,w, "hud_size.mini_zoom", zoom_minimap);		Param(c,w, "hud_size.mini_zoomed", mini_zoomed);
	Param(c,w, "hud_size.mini_rotated", mini_rotated);	Param(c,w, "hud_size.mini_terrain", mini_terrain);
	Param(c,w, "hud_size.arrow", size_arrow);

	//  graphics
	Param(c,w, "graph_detail.anisotropy", anisotropy);		Param(c,w, "graph_detail.view_dist", view_distance);
	Param(c,w, "graph_detail.ter_detail", terdetail);		Param(c,w, "graph_detail.ter_dist", terdist);
	Param(c,w, "graph_detail.road_dist", road_dist);		Param(c,w, "graph_detail.tex_size", tex_size);
	Param(c,w, "graph_detail.ter_mtr", ter_mtr);

	Param(c,w, "graph_par.particles", particles);			Param(c,w, "graph_par.trails", trails);
	Param(c,w, "graph_par.particles_len", particles_len);	Param(c,w, "graph_par.trail_len", trails_len);

	Param(c,w, "graph_reflect.skip_frames", refl_skip);		Param(c,w, "graph_reflect.faces_once", refl_faces);
	Param(c,w, "graph_reflect.map_size", refl_size);		Param(c,w, "graph_reflect.dist", refl_dist);
	Param(c,w, "graph_reflect.mode", refl_mode);
	
	Param(c,w, "graph_shadow.dist", shadow_dist);			Param(c,w, "graph_shadow.size", shadow_size);
	Param(c,w, "graph_shadow.count",shadow_count);			Param(c,w, "graph_shadow.type", shadow_type);
	Param(c,w, "graph_shadow.shaders", shaders);			Param(c,w, "graph_shadow.lightmap_size", lightmap_size);

	Param(c,w, "graph_veget.trees", trees);					Param(c,w, "graph_veget.grass", grass);
	Param(c,w, "graph_veget.trees_dist", trees_dist);		Param(c,w, "graph_veget.grass_dist", grass_dist);
	Param(c,w, "graph_veget.use_imposters", use_imposters);

	//  misc
	Param(c,w, "misc.autostartgame", autostart);
	Param(c,w, "misc.ogredialog", ogre_dialog);		Param(c,w, "misc.escquit", escquit);
	Param(c,w, "misc.bulletDebug", bltDebug);		Param(c,w, "misc.bulletLines", bltLines);
	Param(c,w, "misc.bulletProfilerTxt", bltProfilerTxt);
	Param(c,w, "misc.language", language);			Param(c,w, "misc.loadingback", loadingbackground);
	Param(c,w, "misc.version", version);			Param(c,w, "misc.x11_capture_mouse", x11_capture_mouse);

	Param(c,w, "replay.rec", rpl_rec);				Param(c,w, "replay.ghost", rpl_ghost);
	Param(c,w, "replay.bestonly", rpl_bestonly);	Param(c,w, "replay.listview", rpl_listview);
	Param(c,w, "replay.alpha", rpl_alpha);			Param(c,w, "replay.ghostpar", rpl_ghostpar);
	
	Param(c,w, "sim.game_freq", game_fq);			Param(c,w, "sim.multi_thr", multi_thr);
	Param(c,w, "sim.bullet_freq", blt_fq);			Param(c,w, "sim.bullet_iter", blt_iter);
	Param(c,w, "sim.dynamics_iter", dyn_iter);
	
	Param(c,w, "sound.volume", vol_master);			Param(c,w, "sound.vol_engine", vol_engine);
	Param(c,w, "sound.vol_tires", vol_tires);		Param(c,w, "sound.vol_env", vol_env);

	//  video
	Param(c,w, "video_eff.all_effects", all_effects);
	Param(c,w, "video_eff.bloom", bloom);				Param(c,w, "video_eff.bloomintensity", bloomintensity);
	Param(c,w, "video_eff.bloomorig", bloomorig);		Param(c,w, "video_eff.hdr", hdr);
	Param(c,w, "video_eff.motionblur", motionblur);		Param(c,w, "video_eff.motionblurintensity", motionblurintensity);
	Param(c,w, "video_eff.ssao", ssao);

	Param(c,w, "video.windowx", windowx);			Param(c,w, "video.windowy", windowy);
	Param(c,w, "video.fullscreen", fullscreen);		Param(c,w, "video.vsync", vsync);
	Param(c,w, "video.fsaa", fsaa);					Param(c,w, "video.ssaa", ssaa);
	Param(c,w, "video.buffer", buffer);				Param(c,w, "video.rendersystem", rendersystem);
	
	// not in gui-
	Param(c,w, "misc.boostFromExhaust", boostFromExhaust);
}

SETTINGS::SETTINGS() :  ///  Defaults
	version(100),  // old
	//  track
	track("J1-T"), track_user(false), trackreverse(false),
	//  hud
	show_fps(1), show_gauges(1), trackmap(1),
	show_cam(1), show_times(0), show_digits(1), show_opponents(1),
	car_dbgtxt(0), car_dbgbars(0),
	size_gauges(0.18), size_minimap(0.2), zoom_minimap(1.0),
	mini_zoomed(0), mini_rotated(1), mini_terrain(0),
	tracks_view(0), tracks_sort(0), tracks_sortup(1),
	check_arrow(0), size_arrow(0.2),
	//  graphics
	anisotropy(4),	view_distance(2000), bFog(0),
	terdetail(2), terdist(100), road_dist(1.0), tex_size(1), ter_mtr(2), shaders(0.5),
	refl_skip(200), refl_faces(1), refl_size(0), refl_dist(500.f), refl_mode("single"),
	shadow_type(2), shadow_size(2), shadow_count(3), shadow_dist(3000), lightmap_size(0),
	trees(1.f), grass(1.f), trees_dist(1.f), grass_dist(1.f), use_imposters(true),
	particles(true), trails(true), particles_len(1.f), trails_len(1.f),
	//  car
	abs(0), tcs(0), autoshift(1), autorear(1), rear_inv(1), show_mph(0),
	//  game
	collis_veget(true), collis_cars(false),
	local_players(1), num_laps(2),
	split_vertically(true), language(""), // "" = autodetect lang
	boost_type(2), flip_type(1), boost_power(1.f),
	//  misc
	autostart(0), ogre_dialog(0), escquit(0),
	bltDebug(0), bltLines(1),  bltProfilerTxt(0),
	loadingbackground(true), x11_capture_mouse(false),
	boostFromExhaust(0),
	//  replay
	rpl_rec(1), rpl_ghost(1), rpl_bestonly(1),
	rpl_alpha(0), rpl_ghostpar(0), rpl_listview(0),
	//  sim
	game_fq(82.f), blt_fq(160.f), blt_iter(24), dyn_iter(30), multi_thr(0),
	//  sound
	vol_master(1.f), vol_engine(1.f), vol_tires(1.f), vol_env(1.f),
	//  video
	windowx(800), windowy(600),
	fullscreen(false), vsync(false),
	rendersystem("OpenGL Rendering Subsystem"),
	buffer("FBO"), fsaa(0), ssaa(true),
	//  video eff
	all_effects(false),
	bloom(false), bloomintensity(0.13), bloomorig(0.9), hdr(false),
	motionblur(false), motionblurintensity(0.3),
	ssao(false)
{
	for (int i=0; i < 4; ++i)
	{	car[i] = "ES";  car_hue[i] = 0.4f+0.2f*i;  car_sat[i] = 1.f;  car_val[i] = 1.f;  cam_view[0] = 9;  }
}

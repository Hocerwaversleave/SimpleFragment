/*
 * main.cpp
 *
 *  Created on: 17.05.2017
 *      Author: p.landsteiner
 */
#include "main.h"

using namespace WINDOW_SETUP;
using namespace INTERACTING;
using namespace SHADING;
using namespace FONTING;
using namespace JSON_PARSING;

// git is awesome 2
// RAndom comment

// new comment

// new line

int main() {
	Program_Config program_config{get_Program_Config()};
	if (program_config.hide_console)
		(void)ShowWindow(GetConsoleWindow(), SW_HIDE);

	Window window{"Visualisierung"};
	if (!window.get_Handle()) {
		(void)system((MISSING_MONITOR).c_str());
		glfwTerminate();
		return -1;
	}
	Callback_Resource callback_resource{false, false};
	glfwSetWindowUserPointer(window.get_Handle(), &callback_resource);
	GLint w_width, w_height;
	glfwGetFramebufferSize(window.get_Handle(), &w_width, &w_height);
	glViewport(0, 0, w_width, w_height);

	Shader main_shader{"shaders/shader.vs", "shaders/shader.fs"};
	Shader font_shader{"shaders/font_shader.vs", "shaders/font_shader.fs"};
	font_shader.use_Shader();
	setupFontShader(font_shader, w_width, w_height);
	Fonter fonter{};
	fonter.setupFonter(glGetAttribLocation(font_shader.program_id, "vertex"));
	const Text_Config text_config{get_Text_Config(w_height)};

	main_shader.use_Shader();

	MODEL_LOADING::Loader model_loader{w_width, w_height, main_shader};
	if (!model_loader.loadPictureModel("Bildeinstellungen.json", "main")) {
		model_loader.releaseResources();
		glfwTerminate();
		return -1;
	}
	if (!model_loader.loadPictureModel("Logoeinstellungen.json", "sub")) {
		model_loader.releaseResources();
		glfwTerminate();
		return -1;
	}

	const unsigned picture_count{model_loader.getPictureModelReference("main").pictures};

	TEXTURE_LOADING::Loader texture_loader{"main", 0U, picture_count};
	texture_loader.loadTexturesForAliasWithPaths("main", STARTUP_PATHS);
	texture_loader.loadTexturesForAliasWithPaths("sub", LOGO_PATH);

	PICTURE_DRAWING::Drawer picture_drawer{main_shader};

	FILESYSTEMING::setupCommonPaths();
	FILESYSTEMING::setupImageCounts();

	FS_CLEANING::DirCleaner dir_cleaner{};
	dir_cleaner.clean_2D_Dirs();
	dir_cleaner.clean_3D_Dirs();
	(void)dir_cleaner.wasDirty();

	FILE_MOVING::Mover file_mover{};
	FILE_RENAMING::Renamer file_renamer{picture_count};
	PICTURE_FINDING::Finder picture_finder{picture_count};
	DIR_INDEXING::Indexer dir_indexer{};
	dir_indexer.mymap = LOGGING::retrieveDirIndex();

	PROFINETING::CIFX_Profinet cifx_io{};
	if(!cifx_io.valid_startup)
		glfwSetWindowShouldClose(window.get_Handle(), GLFW_TRUE);
	if (!JSON_PARSING::ConfigIsValid::program)
		glfwSetWindowShouldClose(window.get_Handle(), GLFW_TRUE);
	if (!JSON_PARSING::ConfigIsValid::text)
		glfwSetWindowShouldClose(window.get_Handle(), GLFW_TRUE);
	if (!JSON_PARSING::ConfigIsValid::path)
		glfwSetWindowShouldClose(window.get_Handle(), GLFW_TRUE);
	if (!JSON_PARSING::ConfigIsValid::selection)
		glfwSetWindowShouldClose(window.get_Handle(), GLFW_TRUE);
	if (!JSON_PARSING::ConfigIsValid::controller)
		glfwSetWindowShouldClose(window.get_Handle(), GLFW_TRUE);

	PROFINETING::ProfinetData pns_packet = cifx_io.run_Communication_Cycle();
	std::this_thread::sleep_for(std::chrono::milliseconds(200U));
	pns_packet = cifx_io.run_Communication_Cycle();
	std::string move_serial{pns_packet.mv_rn};
	std::string visual_serial{pns_packet.visual};
	bool _2d_cam_toggle{pns_packet.flag_2d};
	bool _3d_cam_toggle{pns_packet.flag_3d};

	std::string visual_type{"START"};
	std::string visual_charge{"NEMAK"};
	std::string visual_tool{"VISUALISIERUNG"};
	std::string current_subdir{};

	std::string last_date{LOGGING::retrieve_Date()};
	unsigned folder_count{LOGGING::retrieve_Count() + 1U};

	bool search_pictures{};
	bool failed_cycle{};

	while(!glfwWindowShouldClose(window.get_Handle())) {
		std::this_thread::sleep_for(std::chrono::milliseconds(program_config.cycle_timeout));
		pns_packet = cifx_io.run_Communication_Cycle();

		glfwPollEvents();
	    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	    glClear(GL_COLOR_BUFFER_BIT);

	    picture_drawer.drawPictures(model_loader.getPictureModelReference("main"), texture_loader.getTextureInfoReference("main"));
	    picture_drawer.drawPictures(model_loader.getPictureModelReference("sub"), texture_loader.getTextureInfoReference("sub"));

	    fonter.render_Text(font_shader, visual_type, text_config.elements[0]);
	    fonter.render_Text(font_shader, visual_charge, text_config.elements[1]);
	    fonter.render_Text(font_shader, visual_tool, text_config.elements[2]);
	    fonter.render_Text(font_shader, visual_serial, text_config.elements[3]);

	    if (pns_packet.dmc_error) {
	    	glClearColor(text_config.error_bg_red, text_config.error_bg_green, text_config.error_bg_blue, 1.0f);
	    	glClear(GL_COLOR_BUFFER_BIT);
	    	fonter.render_Text(font_shader, DMC_ERROR, text_config.elements[4]);
	    }
	    if (failed_cycle) {
			glClearColor(text_config.error_bg_red, text_config.error_bg_green, text_config.error_bg_blue, 1.0f);
	  		glClear(GL_COLOR_BUFFER_BIT);
	  		fonter.render_Text(font_shader, IMAGE_ERROR, text_config.elements[4]);
	    }

	    glfwSwapBuffers(window.get_Handle());
	    main_shader.use_Shader();

	    if (pns_packet.flag_2d != _2d_cam_toggle) {
	    	_2d_cam_toggle = pns_packet.flag_2d;
	    	move_serial = pns_packet.mv_rn;

	    	const std::string new_date{DATENTIME::get_Date()};
	    	if (new_date != last_date) {
	    		last_date = new_date;
	    		folder_count = 1U;
	    		LOGGING::log_Date(last_date);
	    		LOGGING::log_Count(folder_count);
	    	}
	    	current_subdir = last_date + '/' + std::to_string(folder_count) + '_' + move_serial;
	    	DIRECTORY_CREATING::createSubdirs(current_subdir);
	    	std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT));
	    	file_mover.move2D_FilesToFolder(current_subdir);
	    	std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT));
	    	DIRECTORY_DELETING::delte_2D_Dirs();
	    	dir_cleaner.clean_2D_Dirs();
	    }
	    else if (pns_packet.flag_3d != _3d_cam_toggle) {
	    	_3d_cam_toggle = pns_packet.flag_3d;

	    	std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT));
	    	file_mover.move3D_FilesToFolder(current_subdir);
	    	std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT));
	    	DIRECTORY_DELETING::delte_3D_Dirs();
	    	dir_cleaner.clean_3D_Dirs();

	    	if (!(file_mover.missingFiles() || dir_cleaner.wasDirty())) {
	    		dir_indexer.setKey(move_serial, current_subdir);
	    		LOGGING::logDirIndex(dir_indexer.mymap);
	    		file_renamer.rename_BMPS(current_subdir, move_serial);
				std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT));
				file_renamer.rename_JPGS(current_subdir, pns_packet);
				FS_CLEANING::deleteJPGS(current_subdir);
	    	}
	    	STATISTIK_WRITING::logPartInfo(current_subdir, pns_packet);
	    	folder_count++;
	    }
	    if (pns_packet.visual != visual_serial && pns_packet.visual != "0") {
	    	visual_serial = pns_packet.visual;
	    	search_pictures = true;
	    	visual_type = pns_packet.type_p1 + pns_packet.type_p2;
	    	if (pns_packet.charge_tool.size() >= 2) {
	    		visual_charge = pns_packet.charge_tool.substr(0, pns_packet.charge_tool.size() - 2);
	    		visual_tool = pns_packet.charge_tool.substr(pns_packet.charge_tool.size() - 2, 2);
	    	} else {
	    		visual_charge.clear();
	    		visual_tool.clear();
	    	}
	    }

	    if (search_pictures) {
	    	search_pictures = false;
	    	std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT));
	    	if (dir_indexer.mymap.find(visual_serial) != dir_indexer.mymap.end()) {
	    		failed_cycle = false;
	    		const std::vector<std::string> new_paths {
	    			picture_finder.gatherPicturePaths(dir_indexer.mymap[visual_serial], visual_serial)
	    		};
	    		texture_loader.loadTexturesForAliasWithPaths("main", new_paths);
	    	} else
	    		failed_cycle = true;
	    }

	    if (callback_resource.WINDOWED_WINDOW) {
	    	callback_resource.WINDOWED_WINDOW = false;
	    	window.change_To_Windowed();
	    }
	    if (callback_resource.FULLSCREEN_WINDOW) {
	    	callback_resource.FULLSCREEN_WINDOW = false;
	    	window.change_To_Fullscreen();
	    }
	}

	cifx_io.CIFX_Terminate();
	texture_loader.releaseResources();
	model_loader.releaseResources();
	glfwTerminate();
}

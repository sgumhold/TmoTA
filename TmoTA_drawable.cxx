#include "TmoTA_drawable.h"
#include <cgv/utils/file.h>
#include <cgv/utils/dir.h>
#include <cgv/math/ftransform.h>
#include <cgv/media/image/image_reader.h>

std::string get_json_extension(json_extension ext)
{
	static const char* ext_strs[] = { "json", "bson", "cbor", "ubj", "bjd" };
	return ext_strs[(int)ext];
}


void TmoTA_drawable::glob(const std::string& filter, std::vector<std::string>& file_names)
{
	void* handle = cgv::utils::file::find_first(filter);
	while (handle) {
		file_names.push_back(cgv::utils::file::find_name(handle));
		handle = cgv::utils::file::find_next(handle);
	}
}

bool TmoTA_drawable::open_directory(const std::string& path, const std::string& ext)
{
	this->path = path;
	// extract image file names
	std::vector<std::string> _file_names;
	glob(path + "/*." + ext, _file_names);
	if (_file_names.empty())
		return false;
	// read first file
	cgv::media::image::image_reader ir(df);
	if (!ir.open(path + "/" + _file_names.front()))
		return false;
	if (!ir.read_image(dv)) {
		ir.close();
		return false;
	}
	ir.close();
	// extract essential information
	file_names = _file_names;
	size_t nr_files = configure_video_storage(df.get_component_format(), df.get_width(), df.get_height(), file_names.size());
	if (nr_files < file_names.size())
		file_names.erase(file_names.begin() + nr_files, file_names.end());
	file_idx = 0;
	video_format.set_depth(1);
	video_data.set_ptr(dv.get_ptr<char>());
	return true;
}

TmoTA_drawable::rgb TmoTA_drawable::get_clear_color() const
{
	return rgb(0.75f, 0.75f, 0.75f);
}
void TmoTA_drawable::draw_colored_geometry(cgv::render::context& ctx, cgv::render::shader_program& prog, GLenum prim_type, const std::vector<vec3>& P, const std::vector<rgb>& C)
{
	if (P.empty())
		return;
	int pos_idx = prog.get_position_index();
	int col_idx = prog.get_color_index();
	cgv::render::attribute_array_binding::set_global_attribute_array(ctx, pos_idx, P);
	cgv::render::attribute_array_binding::set_global_attribute_array(ctx, col_idx, C);
	cgv::render::attribute_array_binding::enable_global_array(ctx, pos_idx);
	cgv::render::attribute_array_binding::enable_global_array(ctx, col_idx);
	glDrawArrays(prim_type, 0, (GLsizei)P.size());
	cgv::render::attribute_array_binding::disable_global_array(ctx, pos_idx);
	cgv::render::attribute_array_binding::disable_global_array(ctx, col_idx);
}
/// draw the current video frame with the current object inversely marked
void TmoTA_drawable::draw_texture(cgv::render::context& ctx, int object_idx, int appearance_idx, unsigned frame_idx)
{
	// extract available bounds of current object rectangle and object color
	rgb color(1, 0, 0);
	bool use_min_x = false;
	bool use_max_x = false;
	bool use_min_y = false;
	bool use_max_y = false;
	float min_x = 0;
	float max_x = 0;
	float min_y = 0;
	float max_y = 0;
	if (!objects.empty() && object_idx != -1) {
		const auto& obj = objects[object_idx];
		color = get_object_color(obj.type);
		if (!obj.appearences.empty() && appearance_idx != -1) {
			const auto& app = obj.appearences[appearance_idx];
			use_min_x = extract_value(app.x_min_values, frame_idx, min_x);
			use_max_x = extract_value(app.x_max_values, frame_idx, max_x);
			use_min_y = extract_value(app.y_min_values, frame_idx, min_y);
			use_max_y = extract_value(app.y_max_values, frame_idx, max_y);
		}
	}

	// ensure texture and shader program are available
	if (use_video_tex) {
		if (!video_tex.is_created())
			return;
	}
	else {
		if (!frame_tex.is_created())
			return;
	}
	if (!tex_prog.is_linked())
		return;
	// enable program and 
	tex_prog.enable(ctx);
	ctx.set_color(color);
	// round values in case only integer values for rectangle bounds are allowed
	if (!allow_subpixel_precision) {
		min_x = floor(min_x);
		max_x = floor(max_x) + 1;
		min_y = floor(min_y);
		max_y = floor(max_y) + 1;
	}
	// transmit all uniform parameters to shader program
	if (use_video_tex)
		video_tex.enable(ctx);
	else
		frame_tex.enable(ctx);
	tex_prog.set_uniform(ctx, "video_tex", 0);
	tex_prog.set_uniform(ctx, "frame_tex", 0);
	tex_prog.set_uniform(ctx, "use_video_tex", use_video_tex);
	tex_prog.set_uniform(ctx, "rectangle_border_width", rectangle_border_width);
	tex_prog.set_uniform(ctx, "rectangle_border_gamma", rectangle_border_gamma);
	tex_prog.set_uniform(ctx, "mix_factor", mix_factor);
	tex_prog.set_uniform(ctx, "use_min_x", use_min_x);
	tex_prog.set_uniform(ctx, "use_max_x", use_max_x);
	tex_prog.set_uniform(ctx, "use_min_y", use_min_y);
	tex_prog.set_uniform(ctx, "use_max_y", use_max_y);
	tex_prog.set_uniform(ctx, "min_x", min_x);
	tex_prog.set_uniform(ctx, "max_x", max_x);
	tex_prog.set_uniform(ctx, "min_y", min_y);
	tex_prog.set_uniform(ctx, "max_y", max_y);
	tex_prog.set_uniform(ctx, "x_over_mode", x_over_mode);
	tex_prog.set_uniform(ctx, "y_over_mode", y_over_mode);
	tex_prog.set_uniform(ctx, "aspect", aspect);
	tex_prog.set_uniform(ctx, "width", (int)width);
	tex_prog.set_uniform(ctx, "height", (int)height);
	tex_prog.set_uniform(ctx, "fst_pixel_idx", (int)fst_pixel_idx);
	tex_prog.set_uniform(ctx, "texcoord_w", (float(frame_idx - fst_frame_idx) + 0.5f) / nr_frames);
	// construct geometry of to be rendered rectangle consisting of position and texcoord vectors
	std::vector<vec3> P;
	std::vector<vec2> T;
	P.push_back(vec3(-aspect, -1.0f, 0.0f)); T.push_back(vec2(0.0f, 1.0f));
	P.push_back(vec3(aspect, -1.0f, 0.0f)); T.push_back(vec2(1.0f, 1.0f));
	P.push_back(vec3(-aspect, 1.0f, 0.0f)); T.push_back(vec2(0.0f, 0.0f));
	P.push_back(vec3(aspect, 1.0f, 0.0f)); T.push_back(vec2(1.0f, 0.0f));
	// set attributes of shader program to geometry vectors
	int pi = tex_prog.get_position_index();
	int ti = tex_prog.get_texcoord_index();
	cgv::render::attribute_array_binding::set_global_attribute_array(ctx, pi, P);
	cgv::render::attribute_array_binding::set_global_attribute_array(ctx, ti, T);
	// render rectangle
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	// recover previous state of context
	cgv::render::attribute_array_binding::disable_global_array(ctx, pi);
	cgv::render::attribute_array_binding::disable_global_array(ctx, ti);
	if (use_video_tex)
		video_tex.disable(ctx);
	else
		frame_tex.disable(ctx);
	tex_prog.disable(ctx);
}

void TmoTA_drawable::draw_rectangles(cgv::render::context& ctx, int object_idx, unsigned frame_idx)
{
	if (use_video_tex) {
		if (!video_tex.is_created())
			return;
	}
	else {
		if (!frame_tex.is_created())
			return;
	}
	if (!rect_prog.is_linked())
		return;
	std::vector<vec4> P, P2;
	std::vector<rgb> C, C2;
	for (size_t oi = 0; oi < objects.size(); ++oi) {
		if (oi == object_idx)
			continue;
		auto& obj = objects[oi];
		for (auto& app : obj.appearences) {
			if ((int)frame_idx >= app.fst_frame_idx && (int)frame_idx <= app.lst_frame_idx) {
				std::vector<control_value>::iterator iter;
				vec4 p;
				if (!extract_value(app.x_min_values, frame_idx, p[0]))
					p[0] = float(fst_pixel_idx);
				if (!extract_value(app.x_max_values, frame_idx, p[2]))
					p[2] = float(width - 1 + fst_pixel_idx);
				if (!extract_value(app.y_min_values, frame_idx, p[1]))
					p[1] = float(fst_pixel_idx);
				if (!extract_value(app.y_max_values, frame_idx, p[3]))
					p[3] = float(height - 1 + fst_pixel_idx);

				if (oi == over_object_index) {
					P2.push_back(p);
					C2.push_back(get_object_color(obj.type));
				}
				else {
					P.push_back(p);
					C.push_back(get_object_color(obj.type));
				}
				break;
			}
		}
	}
	if (P.empty() && P2.empty())
		return;

	rect_prog.enable(ctx);
	if (use_video_tex)
		video_tex.enable(ctx);
	else
		frame_tex.enable(ctx);
	rect_prog.set_uniform(ctx, "video_tex", 0);
	rect_prog.set_uniform(ctx, "frame_tex", 0);
	rect_prog.set_uniform(ctx, "use_video_tex", use_video_tex);
	rect_prog.set_uniform(ctx, "rectangle_border_width", rectangle_border_width);
	rect_prog.set_uniform(ctx, "mix_factor", std::min(0.5f, 2.0f * mix_factor));
	rect_prog.set_uniform(ctx, "aspect", aspect);
	rect_prog.set_uniform(ctx, "width", (int)width);
	rect_prog.set_uniform(ctx, "height", (int)height);
	rect_prog.set_uniform(ctx, "allow_subpixel_precision", allow_subpixel_precision);
	rect_prog.set_uniform(ctx, "fst_pixel_idx", (int)fst_pixel_idx);
	rect_prog.set_uniform(ctx, "texcoord_w", (float(frame_idx - fst_frame_idx) + 0.5f) / nr_frames);
	int pi = rect_prog.get_position_index();
	int ci = rect_prog.get_color_index();
	cgv::render::attribute_array_binding::enable_global_array(ctx, pi);
	cgv::render::attribute_array_binding::enable_global_array(ctx, ci);

	if (!P.empty()) {
		rect_prog.set_uniform(ctx, "rectangle_border_gamma", rectangle_border_gamma);
		cgv::render::attribute_array_binding::set_global_attribute_array(ctx, pi, P);
		cgv::render::attribute_array_binding::set_global_attribute_array(ctx, ci, C);
		glDrawArrays(GL_POINTS, 0, (GLsizei)P.size());
	}
	if (!P2.empty()) {
		rect_prog.set_uniform(ctx, "rectangle_border_gamma", 0.3f*rectangle_border_gamma);
		cgv::render::attribute_array_binding::set_global_attribute_array(ctx, pi, P2);
		cgv::render::attribute_array_binding::set_global_attribute_array(ctx, ci, C2);
		glDrawArrays(GL_POINTS, 0, (GLsizei)P2.size());
	}

	cgv::render::attribute_array_binding::disable_global_array(ctx, pi);
	cgv::render::attribute_array_binding::disable_global_array(ctx, ci);

	if (use_video_tex)
		video_tex.disable(ctx);
	else
		frame_tex.disable(ctx);
	rect_prog.disable(ctx);
}
void TmoTA_drawable::add_rectangle(float x0, float y0, float x1, float y1, const rgb& col, std::vector<vec3>& P, std::vector<rgb>& C)
{
	P.push_back(vec3(x1, y1, 0)); C.push_back(col);
	P.push_back(vec3(x0, y1, 0)); C.push_back(col);
	P.push_back(vec3(x0, y0, 0)); C.push_back(col);
	P.push_back(vec3(x1, y1, 0)); C.push_back(col);
	P.push_back(vec3(x0, y0, 0)); C.push_back(col);
	P.push_back(vec3(x1, y0, 0)); C.push_back(col);

}
float TmoTA_drawable::frame_idx_to_x(int frame_idx) const
{
	return object_width_scale * ((2.0f * (frame_idx - fst_frame_idx)) / nr_frames - 1.0f);
}
void TmoTA_drawable::draw_object_time_line(cgv::render::context& ctx, int object_idx, int appearance_idx, unsigned frame_idx)
{
	auto& prog = ctx.ref_default_shader_program(false);
	prog.enable(ctx);

	std::vector<vec3> P;
	std::vector<rgb> C;

	add_rectangle(-1, -1, 1, 1, rgb(0.5f, 0.5f, 0.5f), P, C);
	add_rectangle(-object_width_scale, -1, object_width_scale, 1, rgb(0.7f, 0.7f, 0.7f), P, C);
	if (!objects.empty() && object_idx != -1) {
		float y0 = 1.0f - (2.0f * object_idx) / objects.size();
		float y1 = 1.0f - 2.0f * (object_idx + 1.0f) / objects.size();
		float x0 = frame_idx_to_x(fst_frame_idx);
		float x1 = frame_idx_to_x(lst_frame_idx() + 1);
		add_rectangle(x0, y0, x1, y1, rgb(1, 1, 1), P, C);
		if (appearance_idx != -1) {
			auto& app = objects[object_idx].appearences[appearance_idx];
			float y0 = 1.0f - 2.0f * (object_idx + 0.1f) / objects.size();
			float y1 = 1.0f - 2.0f * (object_idx + 0.9f) / objects.size();
			float x0 = frame_idx_to_x(app.fst_frame_idx);
			float x1 = frame_idx_to_x(app.lst_frame_idx + 1);
			add_rectangle(x0, y0, x1, y1, rgb(0.7f, 0.7f, 0.5f), P, C);
		}
	}
	draw_colored_geometry(ctx, prog, GL_TRIANGLES, P, C);

	P.clear();
	C.clear();
	C.push_back(rgb(1, 0, 0));
	C.push_back(rgb(1, 0, 0));
	float x_t = frame_idx_to_x(frame_idx);
	P.push_back(vec3(x_t, -1, 0));
	P.push_back(vec3(x_t, 1, 0));
	if (nr_read_frames < nr_frames) {
		C.push_back(rgb(0, 1, 0));
		C.push_back(rgb(0, 1, 0));
		float x_t = frame_idx_to_x((int)nr_read_frames);
		P.push_back(vec3(x_t, -1, 0));
		P.push_back(vec3(x_t, 1, 0));
	}
	draw_colored_geometry(ctx, prog, GL_LINES, P, C);

	P.clear();
	C.clear();
	for (size_t oi = 0; oi < objects.size(); ++oi) {
		const auto& obj = objects[oi];
		rgb col = get_object_color(obj.type);
		float y0 = 1.0f - 2.0f * (oi + 0.2f) / objects.size();
		float y1 = 1.0f - 2.0f * (oi + 0.8f) / objects.size();
		for (const auto& app : obj.appearences) {
			float x0 = frame_idx_to_x(app.fst_frame_idx);
			float x1 = frame_idx_to_x(app.lst_frame_idx);
			add_rectangle(x0, y0, x1, y1, col, P, C);
		}
	}
	draw_colored_geometry(ctx, prog, GL_TRIANGLES, P, C);
	prog.disable(ctx);
}
/// initialize all members
TmoTA_drawable::TmoTA_drawable() : video_tex("[R,G,B]"), frame_tex("[R,G,B]")
{
	view_focus = vec2(0.0f, 0.0f);
	view_scale = 1.0f;
	file_idx = 0;
	width = height = nr_frames = 0;
	
	mix_factor = 0.3f;

	rectangle_border_width = 8.0f;
	rectangle_border_gamma = 1.5f;
}

/// called once to init render objects
bool TmoTA_drawable::init(cgv::render::context& ctx)
{
	// tell context to only do minimal stuff on its own
	ctx.set_default_render_pass_flags(
		cgv::render::RenderPassFlags(
			cgv::render::RPF_DRAWABLES_INIT_FRAME |
			cgv::render::RPF_DRAWABLES_DRAW |
			cgv::render::RPF_DRAWABLES_FINISH_FRAME |
			cgv::render::RPF_DRAW_TEXTUAL_INFO)
	);
	if (!tex_prog.build_program(ctx, "texture_shader.glpr")) {
		ctx.error("could not build shader program texture_shader.glpr", &tex_prog);
		return false;
	}
	if (!rect_prog.build_program(ctx, "rectangle_shader.glpr")) {
		ctx.error("could not build shader program rectangle_shader.glpr", &rect_prog);
		return false;
	}
	return true;
}
/// destruct all render objects
void TmoTA_drawable::clear(cgv::render::context& ctx)
{
	if (video_tex.is_created())
		video_tex.destruct(ctx);
	if (frame_tex.is_created())
		frame_tex.destruct(ctx);
	rect_prog.destruct(ctx);
	tex_prog.destruct(ctx);
}

/// allocate video storage for given format / dimensions and return number of frames that fit into system memory
size_t TmoTA_drawable::configure_video_storage(const cgv::data::component_format& cf, size_t w, size_t h, size_t n)
{
	frame_size = w * h * cf.get_entry_size();
	size_t video_storage_size = n * frame_size;
	if (video_storage_size > max_video_storage_size)
		n = max_video_storage_size / frame_size;
	video_cf = cf;
	width = unsigned(w);
	height = unsigned(h);
	aspect = (float)width / height;
	nr_frames = unsigned(n);
	nr_read_frames = 0;
	nr_video_tex_frames = 0;
	video_format.set_component_format(cf);
	video_format.set_width(width);
	video_format.set_height(height);
	video_format.set_depth(1);
	video_data = cgv::data::data_view(&video_format, 0);
	frame_format.set_component_format(cf);
	frame_format.set_width(width);
	frame_format.set_height(height);
	if (video_storage.empty() || video_storage.get_dimensions()(0) != width || video_storage.get_dimensions()(1) != height || video_storage.get_dimensions()(2) != nr_frames) {
		video_storage.get_format().set_component_format(cf);
		video_storage.resize(cgv::media::volume::volume::dimension_type(width, height, nr_frames));
		video_storage.ref_extent() = vec3(1, 1, 1);
	}
	return n;
}
/// clear frame buffer and recreate 3D texture or if necessary upload image files to 3D texture
void TmoTA_drawable::init_frame(cgv::render::context& ctx)
{
	// clear frame buffer
	rgb clear_color = get_clear_color();
	glClearColor(clear_color[0], clear_color[1], clear_color[2], 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	// ensure that currently used texture is of appropriate size
	if (use_video_tex) {
		frame_tex.destruct(ctx);
		if (width > 0 && (!video_tex.is_created() || video_tex.get_width() != width || video_tex.get_height() != height || video_tex.get_depth() != nr_frames)) {
			video_tex.destruct(ctx);
			video_tex.create(ctx, cgv::render::TT_3D, width, height, nr_frames);
			video_tex.set_mag_filter(cgv::render::TF_NEAREST);
			video_tex.set_min_filter(cgv::render::TF_NEAREST);
		}
	}
	else {
		video_tex.destruct(ctx);
		nr_video_tex_frames = 0;
		if (width > 0 && (!frame_tex.is_created() || frame_tex.get_component_format() != video_cf || frame_tex.get_width() != width || frame_tex.get_height() != height)) {
			frame_tex.destruct(ctx);
			frame_tex.create(ctx, cgv::render::TT_2D, width, height);
			frame_tex.set_mag_filter(cgv::render::TF_NEAREST);
			frame_tex.set_min_filter(cgv::render::TF_NEAREST);
		}
	}
	// in case of available frame data, copy it into volume
	if (nr_available_frames > 0) {
		std::copy(video_data.get_ptr<uint8_t>(), video_data.get_ptr<uint8_t>() + nr_available_frames * frame_size, video_storage.get_data_ptr<uint8_t>() + frame_size * nr_read_frames);
		nr_read_frames += nr_available_frames;
		nr_available_frames = 0;
	}
	// if video tex used, ensure that it is up to date
	if (use_video_tex && nr_video_tex_frames < nr_read_frames) {
		video_data.set_ptr(video_storage.get_data_ptr<uint8_t>()+nr_video_tex_frames*frame_size);
		video_format.set_depth(unsigned(nr_read_frames - nr_video_tex_frames));
		video_tex.replace(ctx, 0, 0, int(nr_video_tex_frames), video_data, 0);
		nr_video_tex_frames = nr_read_frames;
	}
	// while not all files have been read, read one more per frame
	if (file_idx < file_names.size()) {
		cgv::media::image::image_reader ir(df);
		if (ir.open(path + "/" + file_names[file_idx])) {
			ir.read_image(dv);
			ir.close();
			video_format.set_depth(1);
			video_data.set_ptr(dv.get_ptr<char>());
			nr_available_frames = 1;
		}
		++file_idx;
		post_redraw();
	}
}

/// draw all views
void TmoTA_drawable::draw(cgv::render::context& ctx, int object_idx, int appearance_idx, unsigned frame_idx)
{
	if (!use_video_tex && frame_size > 0)
		frame_tex.replace(ctx, 0, 0, cgv::data::data_view(&frame_format, video_storage.get_data_ptr<uint8_t>() + frame_size * (frame_idx - fst_frame_idx)), 0);
	// compute viewport dimensions
	int w = ctx.get_width();
	int h = ctx.get_height();
	if (vertical_viewport_split) {
		video_viewport = ivec4(panel_border_width, panel_border_width, w - 3 * panel_border_width - object_viewport_extent, h - 2 * panel_border_width);
		object_viewport = ivec4(2 * panel_border_width + video_viewport[2], panel_border_width, object_viewport_extent, video_viewport[3]);
	}
	else {
		video_viewport = ivec4(panel_border_width, 2 * panel_border_width + object_viewport_extent, w - 2 * panel_border_width, h - object_viewport_extent - 3 * panel_border_width);
		object_viewport = ivec4(panel_border_width, panel_border_width, video_viewport[2], object_viewport_extent);
	}
	video_aspect = float(video_viewport[2]) / video_viewport[3];
	object_aspect = float(object_viewport[2]) / object_viewport[3];
	// render top viewport
	ctx.push_window_transformation_array();
	ctx.set_viewport(video_viewport);
	ctx.push_modelview_matrix();
	ctx.set_modelview_matrix(cgv::math::look_at4<float>(vec3(view_focus(0), view_focus(1), 1), vec3(view_focus(0), view_focus(1), 0), vec3(0, 1, 0)));
	ctx.push_projection_matrix();
	ctx.set_projection_matrix(cgv::math::ortho4<float>(-video_aspect / view_scale, video_aspect / view_scale, -1.0f / view_scale, 1.0f / view_scale, -1, 1));
	MPW = ctx.get_modelview_projection_window_matrix();
	draw_texture(ctx, object_idx, appearance_idx, frame_idx);
	if (object_idx == -1 || !focus_current)
		draw_rectangles(ctx, object_idx, frame_idx);
	ctx.pop_projection_matrix();
	ctx.pop_modelview_matrix();

	// render object viewport	
	ctx.set_viewport(object_viewport);
	ctx.push_modelview_matrix();
	ctx.set_modelview_matrix(cgv::math::identity4<double>());
	ctx.push_projection_matrix();
	ctx.set_projection_matrix(cgv::math::identity4<double>());
	draw_object_time_line(ctx, object_idx, appearance_idx, frame_idx);
	ctx.pop_projection_matrix();
	ctx.pop_modelview_matrix();
	ctx.pop_window_transformation_array();
}

#include <cgv/base/register.h>

#ifdef REGISTER_SHADER_FILES
#include <TmoTA_shader_inc.h>
#endif
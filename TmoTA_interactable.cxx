#include "TmoTA_interactable.h"
#include <cgv/gui/mouse_event.h>
#include <cgv/reflect/reflect_enum.h>
#include <cgv_reflect_types/media/color.h>
#include <cgv/gui/dialog.h>
#include <cgv/math/ftransform.h>
#include <cgv/utils/convert_string.h>
#include <cgv/media/volume/sliced_volume_io.h>
#include <cgv/gui/animate.h>
#include <cgv/utils/file.h>
#include <cgv/reflect/reflection_handler.h>

//#define DEBUG_EVENTS


bool modify_max(int mode)
{
	return (mode & 2) != 0;
}
int mod_mode(bool mod_min, bool mod_max)
{
	return (mod_min ? 1 : 0) + (mod_max ? 2 : 0);
}

cgv::reflect::enum_reflection_traits<json_extension> get_reflection_traits(const json_extension&)
{
	return cgv::reflect::enum_reflection_traits<json_extension>("json,bson,cbor,ubj,bjd");
}

/// ensure ui up to date after opening a directory
bool TmoTA_interactable::open_directory(const std::string& path, const std::string& ext)
{
	if (TmoTA_drawable::open_directory(path, ext))
		set_object_file_name(path + "/" + cgv::utils::file::get_file_name(path));
	return true;
}
/// define clear color from impuls definition
TmoTA_interactable::rgb TmoTA_interactable::get_clear_color()const
{
	return (1.0f - impuls_lambda) * rgb(0.75f, 0.75f, 0.75f) + impuls_lambda * impuls_color;
}
/// we start our trigger here
bool TmoTA_interactable::init(cgv::render::context& ctx)
{
	// ensure that timer_event() is called 200 times per second
	trig.schedule_recuring(1.0 / 200);
	return TmoTA_drawable::init(ctx);
}
void TmoTA_interactable::set_object_file_name(const std::string& file_name)
{
	json_file_name = file_name;
	open_object_file(get_json_file_name());
	on_set(&json_file_name);
	object_idx = -1;
	appearance_idx = -1;
	x_min_idx = 0;
	x_max_idx = 0;
	y_min_idx = 0;
	y_max_idx = 0;
	on_set(&object_idx);
	frame_idx = fst_frame_idx;
	on_set(&frame_idx);
}

/// ensure that file index is updated in ui
void TmoTA_interactable::init_frame(cgv::render::context& ctx)
{
	// handle reading of image files from folder with init_frame of TmoTA_drawable and update UI
	size_t old_file_idx = file_idx;
	size_t old_nr_read_frame = nr_read_frames;
	TmoTA_drawable::init_frame(ctx);
	if (old_file_idx != file_idx)
		update_member(&file_idx);
	if (old_nr_read_frame != nr_read_frames)
		update_member(&nr_read_frames);
}

/// specialize draw method to current object and appearance indices
void TmoTA_interactable::draw(cgv::render::context& ctx)
{
	//std::cout << "draw(" << file_idx << ")" << std::endl;
	over_object_index = over_mode.new_object_idx;
	x_over_mode = over_mode.x_mode;
	y_over_mode = over_mode.y_mode;
	TmoTA_drawable::draw(ctx, object_idx, appearance_idx, frame_idx);
}

void TmoTA_interactable::finish_frame(cgv::render::context& ctx)
{
	if (!over_mode.in_video_viewport)
		return;
	bool need_text =
		over_mode.nr_objects > 1 ||
		(over_mode.mode < 1 && over_mode.new_object_idx == -1) ||
		over_mode.mode == 1;
	if (!need_text)
		return;
//	if (over_mode.nr_objects < 2 && (over_mode.mode > 1 || over_mode.new_object_idx != -1))
//		return;
	if (left_clicked || right_clicked)
		return;
	std::stringstream ss;
	int ai = -1;
	bool last = true;
	float text_pos = 0.5f;
	if (over_mode.mode == 1) {
		ss << "A]";
		ai = (int)objects[object_idx].appearences.size() - 1;
		last = true;
	}
	else if (over_mode.nr_objects < 2) {
		if (object_idx != -1) {
			if (-over_mode.mode == objects[object_idx].appearences.size()) {
				if (over_mode.connect_right) {
					ai = -over_mode.mode - 1;
					last = true;
					ss << "A]->";
					text_pos = 1.0f;
				}
				else
					ss << "[A";
			}
			else {
				if (over_mode.mode < 0 && over_mode.connect_right) {
					ai = -over_mode.mode - 1;
					last = true;
					ss << "A]->";
					text_pos = 1.0f;
				}
				else if (over_mode.connect_left) {
					ai = -over_mode.mode;
					last = false;
					ss << "<-[A";
					text_pos = 0.0f;
				}
				else
					ss << "+O";
			}
		}
		else
			ss << "+O";
	}
	else
		ss << ((unsigned)obj_layer % over_mode.nr_objects + 1) << "(" << over_mode.nr_objects << ")";
	std::string s = ss.str();

	// render top viewport
	if (ai != -1) {
		const auto& app = objects[object_idx].appearences[ai];
		vec4 r;
		if (last)
			r = vec4(app.x_min_values.back().value, app.y_min_values.back().value, app.x_max_values.back().value, app.y_max_values.back().value);
		else
			r = vec4(app.x_min_values.front().value, app.y_min_values.front().value, app.x_max_values.front().value, app.y_max_values.front().value);
		rgb c = get_object_color(objects[object_idx].type);
		ctx.push_window_transformation_array();
		ctx.set_viewport(video_viewport);
		ctx.push_modelview_matrix();
		ctx.set_modelview_matrix(cgv::math::look_at4<float>(vec3(view_focus(0), view_focus(1), 1), vec3(view_focus(0), view_focus(1), 0), vec3(0, 1, 0)));
		ctx.push_projection_matrix();
		ctx.set_projection_matrix(cgv::math::ortho4<float>(-video_aspect / view_scale, video_aspect / view_scale, -1.0f / view_scale, 1.0f / view_scale, -1, 1));
		MPW = ctx.get_modelview_projection_window_matrix();

		rect_prog.enable(ctx);
		if (use_video_tex)
			video_tex.enable(ctx);
		else
			frame_tex.enable(ctx);
		rect_prog.set_uniform(ctx, "video_tex", 0);
		rect_prog.set_uniform(ctx, "frame_tex", 0);
		rect_prog.set_uniform(ctx, "use_video_tex", use_video_tex);
		rect_prog.set_uniform(ctx, "rectangle_border_width", 1.0f);
		rect_prog.set_uniform(ctx, "mix_factor", std::min(0.5f, 0.5f * mix_factor));
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
		rect_prog.set_uniform(ctx, "rectangle_border_gamma", 1.0f);
		cgv::render::attribute_array_binding::set_global_attribute_array(ctx, pi, &r, 1);
		cgv::render::attribute_array_binding::set_global_attribute_array(ctx, ci, &c, 1);
		glDrawArrays(GL_POINTS, 0, 1);
		cgv::render::attribute_array_binding::disable_global_array(ctx, pi);
		cgv::render::attribute_array_binding::disable_global_array(ctx, ci);
		if (use_video_tex)
			video_tex.disable(ctx);
		else
			frame_tex.disable(ctx);
		rect_prog.disable(ctx);
		ctx.pop_projection_matrix();
		ctx.pop_modelview_matrix();
		ctx.pop_window_transformation_array();
	}
	if (ff.empty()) {
		cgv::media::font::enumerate_font_names(font_names);
		for (font_idx = 0; font_idx < font_names.size(); ++font_idx)
			if (std::string("Consolas") == font_names[font_idx])
				break;
		if (font_idx == font_names.size())
			for (font_idx = 0; font_idx < font_names.size(); ++font_idx)
				if (std::string("Courier New") == font_names[font_idx])
					break;
		if (font_idx == font_names.size())
			for (font_idx = 0; font_idx < font_names.size(); ++font_idx)
				if (std::string("Arial") == font_names[font_idx])
					break;
		if (font_idx == font_names.size())
			font_idx = 0;
		cgv::media::font::font_ptr f = cgv::media::font::find_font(font_names[font_idx]);
		ff = f->get_font_face((bold ? cgv::media::font::FFA_BOLD : 0) +
			(italic ? cgv::media::font::FFA_ITALIC : 0));
	}
	if (ff.empty())
		return;
	ctx.set_color(font_color);
	ctx.push_pixel_coords();
	ctx.enable_font_face(ff, font_size);
	float text_width = ff->measure_text_width(s, font_size);
	int text_x_pos = std::min(std::max(0, over_mode.mouse_x - (int)(text_pos * text_width)), (int)ctx.get_width()-(int)floor(text_width));
	//float f = (float)over_mode.mouse_x / ctx.get_width();
	ctx.set_cursor(text_x_pos, ctx.get_height() - over_mode.mouse_y + 6 - object_viewport[1]);
	ctx.output_stream() << s.c_str();
	ctx.output_stream().flush();
	ctx.pop_pixel_coords();

}

void TmoTA_interactable::color_impuls(const rgb& col)
{
	impuls_color = col;
	impuls_lambda = 1.0f;
	on_set(&impuls_lambda);
	cgv::gui::animate_with_linear_blend(impuls_lambda, 0.0f, 0.2, 0.2)->set_base_ptr(this);
}
TmoTA_interactable::TmoTA_interactable() : node("Annotation")
{
	object_type_file_name = "Sepia";
	font_size = 16;
	font_color = rgb(1, 1, 1);
	bold = true;
	italic = false;
	font_idx = 0;
	frame_idx = fst_frame_idx;
	span_rectangle_from_center = false;
	impuls_lambda = 0.0f;
	potential_left_click = false;
	potential_right_click = false;
	restrict_animation = false;
	auto_center_view = false;
	last_t = 0;
	forward = true;
	animate = false;
	object_idx = -1;
	appearance_idx = -1;
	fps = 60;
	rectangle_key = 0;
	rectangle_pix = vec2(0.0f);
	x_mode = y_mode = mod_mode(false, false);
	connect(trig.shoot, this, &TmoTA_interactable::timer_event);
}

void TmoTA_interactable::timer_event(double t, double dt)
{
	if (!animate)
		return;
	dt = t - last_t;
	int frame_offset = int(dt * fps);
	//std::cout << "offset = " << frame_offset << std::endl;
	if (frame_offset == 0)
		return;
	last_t = last_t + frame_offset / fps;
	if (forward) {
		if ((int)frame_idx < lst_frame_idx()) {
			frame_idx = std::min(std::min(int(frame_idx +frame_offset), lst_frame_idx()), int(nr_read_frames + fst_frame_idx));
			if (restrict_animation && !objects.empty() && object_idx != -1 && appearance_idx != -1) {
				const auto& app = objects[object_idx].appearences[appearance_idx];
				if ((int)frame_idx >= app.lst_frame_idx) {
					frame_idx = app.lst_frame_idx;
					forward = false;
					update_member(&forward);
				}
			}
			on_set(&frame_idx);
		}
		if ((int)frame_idx >= lst_frame_idx()) {
			frame_idx = lst_frame_idx();
			forward = false;
			update_member(&forward);
		}
	}
	else {
		if (frame_idx > fst_frame_idx) {
			frame_idx = std::max((int)frame_idx - frame_offset, fst_frame_idx);
			if (restrict_animation && !objects.empty() && object_idx != -1 && appearance_idx != -1) {
				const auto& app = objects[object_idx].appearences[appearance_idx];
				if ((int)frame_idx <= app.fst_frame_idx) {
					frame_idx = app.fst_frame_idx;
					forward = true;
					update_member(&forward);
				}
			}
			on_set(&frame_idx);
		}
		else {
			forward = true;
			update_member(&forward);
		}
	}
}

/// return type name
std::string TmoTA_interactable::get_type_name() const
{
	return "TmoTA";
}
/// publish members that should be configurable through config file
bool TmoTA_interactable::self_reflect(cgv::reflect::reflection_handler& rh)
{
	return
		rh.reflect_member("path", path) &&
		rh.reflect_member("video_file_name", video_file_name) &&
		rh.reflect_member("font_color", font_color) &&
		rh.reflect_member("extent_appearance", extent_appearance) &&
		rh.reflect_member("animate", animate) &&
		rh.reflect_member("object_width_scale", object_width_scale) &&
		rh.reflect_member("object_file_extension", object_file_extension) &&
		rh.reflect_member("use_asynchronous_read", use_asynchronous_read) &&
		rh.reflect_member("pick_width", pick_width) &&
		rh.reflect_member("auto_center_view", auto_center_view) &&
		rh.reflect_member("restrict_animation", restrict_animation) &&
		rh.reflect_member("fps", fps) &&
		rh.reflect_member("rectangle_border_gamma", rectangle_border_gamma) &&
		rh.reflect_member("mix_factor", mix_factor) &&
		rh.reflect_member("focus_current", focus_current) &&
		rh.reflect_member("span_rectangle_from_center", span_rectangle_from_center) &&
		rh.reflect_member("object_viewport_extent", object_viewport_extent) &&
		rh.reflect_member("vertical_viewport_split", vertical_viewport_split) &&
		rh.reflect_member("panel_border_width", panel_border_width) &&
		rh.reflect_member("object_type_file_name", object_type_file_name) &&
		rh.reflect_member("rectangle_border_width", rectangle_border_width) &&
		rh.reflect_member("frame_idx", frame_idx);
}

void TmoTA_interactable::on_progress_update_callback(int cnt, void* This)
{
	reinterpret_cast<TmoTA_interactable*>(This)->on_progress_update(cnt);
}

/// allocate video storage for given format / dimensions and return number of frames that fit into system memory
size_t TmoTA_interactable::configure_video_storage(const cgv::data::component_format& cf, size_t w, size_t h, size_t n)
{
	size_t nr_f = TmoTA_drawable::configure_video_storage(cf, w, h, n);
	frame_idx = fst_frame_idx;
	update_member(&width);
	update_member(&height);
	update_member(&nr_frames);
	update_member(&frame_idx);
	configure_controls();
	return nr_f;
}

void TmoTA_interactable::on_progress_update(int cnt)
{
	if (cnt == -1) {
		auto& df = video_storage.get_format();
		frame_size = df.get_entry_size() * df.get_width() * df.get_height();
		size_t max_nr_frames = max_video_storage_size / frame_size;
		if (max_nr_frames < df.get_depth()) {
			df.set_depth(unsigned(max_nr_frames));
			std::cout << "truncated video file to " << max_nr_frames << std::endl;
		}
	}
	else if (cnt == 0) {
		configure_video_storage(video_storage.get_format().get_component_format(), video_storage.get_dimensions()(0), video_storage.get_dimensions()(1), video_storage.get_dimensions()(2));
	}
	else if (cnt <= int(nr_frames)) {
		nr_read_frames = cnt;
		update_member(&nr_read_frames);
		post_redraw();
	}
}

void TmoTA_interactable::read_thread::on_progress_update_callback(int cnt, void* This)
{
	reinterpret_cast<TmoTA_interactable::read_thread*>(This)->on_progress_update(cnt);
}

void TmoTA_interactable::read_thread::on_progress_update(int cnt)
{
	mtx.lock();
	progress = cnt;
	mtx.unlock();
	lti->on_progress_update(cnt);
}

int TmoTA_interactable::read_thread::query_progress() const
{
	int prg = 0;
	mtx.lock();
	prg = progress;
	mtx.unlock();
	return prg;
}

void TmoTA_interactable::read_thread::run()
{
	cgv::media::volume::read_volume_from_video_with_ffmpeg(lti->video_storage, file_name, 
		cgv::media::volume::volume::dimension_type(-1, -1, -1),
		cgv::media::volume::volume::extent_type(-1.0f), cgv::data::component_format(component_format), 0,
		cgv::media::volume::FT_NO_FLIP, &TmoTA_interactable::read_thread::on_progress_update_callback, this, false);
	lti->post_redraw();
}

bool TmoTA_interactable::start_asynchronous_read(const std::string& file_name)
{
	if (!cgv::utils::file::exists(file_name))
		return false;
	if (read_trd) {
		if (read_trd->is_running())
			read_trd->kill();
		read_trd = 0;
	}
	read_trd = new read_thread(this, file_name, "uint8[R,G,B]");
	read_trd->start();
	return true;
}

void TmoTA_interactable::ensure_centered_view()
{
	if (objects.empty() || object_idx == -1 || appearance_idx == -1 || !auto_center_view)
		return;
	const auto& obj = objects[object_idx];
	const auto& app = obj.appearences[appearance_idx];
	float min_x;
	float max_x;
	float min_y;
	float max_y;
	bool use_min_x = extract_value(app.x_min_values, frame_idx, min_x);
	bool use_max_x = extract_value(app.x_max_values, frame_idx, max_x);
	if (use_min_x && use_max_x) {
		float ctr_u = pixel_x_to_texcoord_u_f(0.5f * (min_x + max_x));
		view_focus(0) = texcoord_u_to_model_x(ctr_u);
	}
	bool use_min_y = extract_value(app.y_min_values, frame_idx, min_y);
	bool use_max_y = extract_value(app.y_max_values, frame_idx, max_y);
	if (use_min_y && use_max_y) {
		float ctr_v = pixel_y_to_texcoord_v_f(0.5f * (min_y + max_y));
		view_focus(1) = texcoord_v_to_model_y(ctr_v);
	}
}

/// callback where we handle changes of member variables
void TmoTA_interactable::on_set(void* member_ptr)
{
	if (member_ptr == &object_type_file_name) {
		if (read_object_type_definition(object_type_file_name))
			post_recreate_gui();
	}
	if (member_ptr == &path) {
		if (open_directory(path)) {
			frame_idx = fst_frame_idx;
			update_member(&nr_frames);
			update_member(&frame_idx);
			configure_controls();
		}
	}
	if (member_ptr == &video_file_name) {
		if (use_asynchronous_read ? start_asynchronous_read(video_file_name) :
			cgv::media::volume::read_volume_from_video_with_ffmpeg(video_storage, video_file_name, cgv::media::volume::volume::dimension_type(-1),
				cgv::media::volume::volume::extent_type(-1.0f), cgv::data::component_format("uint8[R,G,B]"), 0,
				cgv::media::volume::FT_NO_FLIP, &TmoTA_interactable::on_progress_update_callback, this)) {
			file_names.clear();
			file_idx = last_file_idx = 0;
			set_object_file_name(cgv::utils::file::drop_extension(video_file_name));
		}
		post_recreate_gui();
	}
	if (member_ptr == &object_idx) {
		if (open_appearance_object_idx != -1 && object_idx != open_appearance_object_idx) {
			auto& apps = objects[open_appearance_object_idx].appearences;
			apps.erase(apps.begin() + open_appearance_idx);
			if (apps.empty()) {
				objects.erase(objects.begin() + open_appearance_object_idx);
				if (object_idx >= open_appearance_object_idx)
					--object_idx;
			}
			open_appearance_object_idx = -1;
			open_appearance_idx = -1;
			post_recreate_gui();
		}
		update_over_mode(over_mode.mouse_x, over_mode.mouse_y, over_mode.in_video_viewport);
		if (object_idx == -1)
			appearance_idx = -1;
		else
			appearance_idx = 0;
		on_set(&appearance_idx);
	}
	if (member_ptr == &appearance_idx) {
		x_min_idx = 0;
		x_max_idx = 0;
		y_min_idx = 0;
		y_max_idx = 0;
		on_set(&x_min_idx);
		on_set(&x_max_idx);
		on_set(&y_min_idx);
		on_set(&y_max_idx);
	}
	if (member_ptr == &x_min_idx || member_ptr == &x_max_idx || member_ptr == &y_min_idx || member_ptr == &y_max_idx) {
		post_recreate_gui();
	}
	if (member_ptr == &frame_idx) {
		update_over_mode(over_mode.mouse_x, over_mode.mouse_y, over_mode.in_video_viewport);
		if (!objects.empty() && object_idx != -1) {
			const auto& obj = objects[object_idx];
			// find appearance index
			int ai;
			bool found = false;
			for (ai = 0; ai < obj.appearences.size(); ++ai) {
				const auto& app = obj.appearences[ai];
				if ((int)frame_idx >= app.fst_frame_idx && (int)frame_idx <= app.lst_frame_idx) {
					found = true;
					break;
				}
			}
			if (!found)
				ai = -1;
			if (ai != appearance_idx) {
				appearance_idx = ai;
				on_set(&appearance_idx);
			}
			ensure_centered_view();
		}
	}
	if (!objects.empty()) {
		for (auto& obj : objects)
			if (member_ptr == &obj.type) {
				if (obj.type != OT_USER_DEFINE) {
					obj.type_name = get_object_type_name(obj.type);
					update_member(&obj.type_name);
				}
			}
			else if (member_ptr == &obj.type_name) {
				bool found = false;
				for (ObjectType ot = ObjectType(1); ot < get_object_type_end(); ++(int&)ot) {
					if (obj.type_name == get_object_type_name(ot)) {
						obj.type = ot;
						update_member(&obj.type);
						found = true;
						break;
					}
					if (!found) {
						obj.type = OT_USER_DEFINE;
						update_member(&obj.type);
					}
				}
			}
		if (object_idx != -1) {
			auto& obj = objects[object_idx];
			if (member_ptr == &obj.type) {
				if (obj.type != OT_USER_DEFINE) {
					obj.type_name = get_object_type_name(obj.type);
					update_member(&obj.type_name);
				}
			}
			if (member_ptr == &obj.type_name) {
				bool found = false;
				for (ObjectType ot = ObjectType(1); ot < get_object_type_end(); ++(int&)ot) {
					if (obj.type_name == get_object_type_name(ot)) {
						obj.type = ot;
						update_member(&obj.type);
						found = true;
						break;
					}
					if (!found) {
						obj.type = OT_USER_DEFINE;
						update_member(&obj.type);
					}
				}
			}
			if (!obj.appearences.empty() && appearance_idx != -1) {
				auto& app = obj.appearences[appearance_idx];
				if (member_ptr == &app.lst_frame_idx) {
					if (app.lst_frame_idx < app.fst_frame_idx) {
						app.fst_frame_idx = app.lst_frame_idx;
						update_member(&app.fst_frame_idx);
					}
					size_t cnt = app.lst_frame_idx - app.fst_frame_idx + 1;
					post_recreate_gui();
				}
				if (member_ptr == &app.fst_frame_idx) {
					if (app.fst_frame_idx > app.lst_frame_idx) {
						app.lst_frame_idx = app.fst_frame_idx;
						update_member(&app.lst_frame_idx);
					}
					size_t cnt = app.lst_frame_idx - app.fst_frame_idx + 1;
				}
			}
		}
	}
	update_member(member_ptr);
	post_redraw();
}

/// ensure that all slider ranges of the ui are set correctly
void TmoTA_interactable::configure_controls()
{
	if (find_control(frame_idx)) {
		find_control(frame_idx)->set("min", fst_frame_idx);
		find_control(frame_idx)->set("max", lst_frame_idx());
	}
	if (find_control(object_idx)) {
		find_control(object_idx)->set("max", objects.size() == 0 ? 0 : objects.size() - 1);
		if (find_control(appearance_idx)) {
			auto& obj = objects[object_idx];
			find_control(appearance_idx)->set("max", obj.appearences.size() == 0 ? 0 : obj.appearences.size() - 1);
			if (appearance_idx != -1) {
				auto& app = obj.appearences[appearance_idx];
				if (find_control(app.fst_frame_idx)) {
					find_control(app.fst_frame_idx)->set("min", fst_frame_idx);
					find_control(app.fst_frame_idx)->set("max", lst_frame_idx());
				}
				if (find_control(app.lst_frame_idx)) {
					find_control(app.lst_frame_idx)->set("min", fst_frame_idx);
					find_control(app.lst_frame_idx)->set("max", lst_frame_idx());
				}
				if (find_control(x_min_idx))
					find_control(x_min_idx)->set("max", app.x_min_values.size() == 0 ? 0 : app.x_min_values.size() - 1);
				if (find_control(x_max_idx))
					find_control(x_max_idx)->set("max", app.x_max_values.size() == 0 ? 0 : app.x_max_values.size() - 1);
				if (find_control(y_min_idx))
					find_control(y_min_idx)->set("max", app.y_min_values.size() == 0 ? 0 : app.y_min_values.size() - 1);
				if (find_control(y_max_idx))
					find_control(y_max_idx)->set("max", app.y_max_values.size() == 0 ? 0 : app.y_max_values.size() - 1);
			}
		}
	}
}

void TmoTA_interactable::create_object_gui()
{
	add_member_control(this, "object_idx", object_idx, "value_slider", "min=-1;max=0;ticks=true;tooltip='navigate with @b <Down|Up>@n\nunselect with@b <Esc>@n\ncreate new with@b <O>@n\ndelete current with@b <Delete>'");
	if (objects.size() > 0 && object_idx != -1) {
		align("\a");
		auto& obj = objects[object_idx];
		add_member_control(this, "type", obj.type, "dropdown", get_object_type_name_enum_decl());
		add_member_control(this, "type_name", obj.type_name);
		add_member_control(this, "comment", obj.comment);
		add_member_control(this, "appearance_idx", appearance_idx, "value_slider", "min=-1;max=0;ticks=true");
		if (obj.appearences.size() > 0 && appearance_idx != -1) {
			align("\a");
			auto& app = obj.appearences[appearance_idx];
			std::string app_frame_slider_options = "ticks=true;min=";
			app_frame_slider_options += cgv::utils::to_string(fst_frame_idx);
			app_frame_slider_options += ";max=";
			app_frame_slider_options += cgv::utils::to_string(lst_frame_idx());
			add_member_control(this, "fst_frame", app.fst_frame_idx, "value_slider", app_frame_slider_options);
			add_member_control(this, "lst_frame", app.lst_frame_idx, "value_slider", app_frame_slider_options);
			std::string frame_idx_slider_options = "ticks=true;min=";
			frame_idx_slider_options += cgv::utils::to_string(app.fst_frame_idx);
			frame_idx_slider_options += ";max=";
			frame_idx_slider_options += cgv::utils::to_string(app.lst_frame_idx);

			std::string x_value_slider_options = "ticks=true;min=0;max=";
			std::string y_value_slider_options = x_value_slider_options;
			x_value_slider_options += cgv::utils::to_string(video_tex.get_width() - 1);
			y_value_slider_options += cgv::utils::to_string(video_tex.get_height() - 1);

			add_member_control(this, "x_min_idx", x_min_idx, "value_slider", "min=0;max=0;ticks=true");
			if (app.x_min_values.size() > 0) {
				align("\a");
				auto& cv = app.x_min_values[x_min_idx];
				add_member_control(this, "frame_idx", cv.frame_idx, "value_slider", frame_idx_slider_options);
				add_member_control(this, "x_min_value", cv.value, "value_slider", x_value_slider_options);
				align("\b");
			}
			add_member_control(this, "x_max_idx", x_max_idx, "value_slider", "min=0;max=0;ticks=true");
			if (app.x_max_values.size() > 0) {
				align("\a");
				auto& cv = app.x_max_values[x_max_idx];
				add_member_control(this, "frame_idx", cv.frame_idx, "value_slider", frame_idx_slider_options);
				add_member_control(this, "x_max_value", cv.value, "value_slider", x_value_slider_options);
				align("\b");
			}
			add_member_control(this, "y_min_idx", y_min_idx, "value_slider", "min=0;max=0;ticks=true");
			if (app.y_min_values.size() > 0) {
				align("\a");
				auto& cv = app.y_min_values[y_min_idx];
				add_member_control(this, "frame_idx", cv.frame_idx, "value_slider", frame_idx_slider_options);
				add_member_control(this, "y_min_value", cv.value, "value_slider", y_value_slider_options);
				align("\b");
			}
			add_member_control(this, "y_max_idx", y_max_idx, "value_slider", "min=0;max=0;ticks=true");
			if (app.y_max_values.size() > 0) {
				align("\a");
				auto& cv = app.y_max_values[y_max_idx];
				add_member_control(this, "frame_idx", cv.frame_idx, "value_slider", frame_idx_slider_options);
				add_member_control(this, "y_max_value", cv.value, "value_slider", y_value_slider_options);
				align("\b");
			}
			align("\b");
		}
		align("\b");
	}
}
void TmoTA_interactable::create_object_gui_2()
{
	for (size_t oi = 0; oi < objects.size(); ++oi) {
		auto& obj = objects[oi];
		bool show = begin_tree_node(cgv::utils::to_string(oi+1, 2, ' '), obj, false, "options='w=40';align=' '");
		add_member_control(this, "type", obj.type_name, "", "w=100", "");
		add_member_control(this, "", obj.type, "dropdown", get_object_type_name_enum_decl()+";w=20");
		if (!show)
			continue;

		align("\a");
		add_member_control(this, "comment", obj.comment);
		for (size_t ai = 0; ai < obj.appearences.size(); ++ai) {
			auto& app = obj.appearences[ai];
			if (begin_tree_node(std::string("Appearance ")+cgv::utils::to_string(ai+1), app, false, "level=4")) {
				align("\a");
				std::string app_frame_slider_options = "ticks=true;min=";
				app_frame_slider_options += cgv::utils::to_string(fst_frame_idx);
				app_frame_slider_options += ";max=";
				app_frame_slider_options += cgv::utils::to_string(lst_frame_idx());
				add_member_control(this, "fst_frame", app.fst_frame_idx, "value_slider", app_frame_slider_options);
				add_member_control(this, "lst_frame", app.lst_frame_idx, "value_slider", app_frame_slider_options);
				std::string frame_idx_slider_options = "ticks=true;min=";
				frame_idx_slider_options += cgv::utils::to_string(app.fst_frame_idx);
				frame_idx_slider_options += ";max=";
				frame_idx_slider_options += cgv::utils::to_string(app.lst_frame_idx);

				std::string x_value_slider_options = "ticks=true;min=0;max=";
				std::string y_value_slider_options = x_value_slider_options;
				x_value_slider_options += cgv::utils::to_string(video_tex.get_width() - 1);
				y_value_slider_options += cgv::utils::to_string(video_tex.get_height() - 1);

				add_member_control(this, "x_min_idx", x_min_idx, "value_slider", "min=0;max=0;ticks=true");
				if (app.x_min_values.size() > 0) {
					align("\a");
					auto& cv = app.x_min_values[x_min_idx];
					add_member_control(this, "frame_idx", cv.frame_idx, "value_slider", frame_idx_slider_options);
					add_member_control(this, "x_min_value", cv.value, "value_slider", x_value_slider_options);
					align("\b");
				}
				add_member_control(this, "x_max_idx", x_max_idx, "value_slider", "min=0;max=0;ticks=true");
				if (app.x_max_values.size() > 0) {
					align("\a");
					auto& cv = app.x_max_values[x_max_idx];
					add_member_control(this, "frame_idx", cv.frame_idx, "value_slider", frame_idx_slider_options);
					add_member_control(this, "x_max_value", cv.value, "value_slider", x_value_slider_options);
					align("\b");
				}
				add_member_control(this, "y_min_idx", y_min_idx, "value_slider", "min=0;max=0;ticks=true");
				if (app.y_min_values.size() > 0) {
					align("\a");
					auto& cv = app.y_min_values[y_min_idx];
					add_member_control(this, "frame_idx", cv.frame_idx, "value_slider", frame_idx_slider_options);
					add_member_control(this, "y_min_value", cv.value, "value_slider", y_value_slider_options);
					align("\b");
				}
				add_member_control(this, "y_max_idx", y_max_idx, "value_slider", "min=0;max=0;ticks=true");
				if (app.y_max_values.size() > 0) {
					align("\a");
					auto& cv = app.y_max_values[y_max_idx];
					add_member_control(this, "frame_idx", cv.frame_idx, "value_slider", frame_idx_slider_options);
					add_member_control(this, "y_max_value", cv.value, "value_slider", y_value_slider_options);
					align("\b");
				}
				align("\b");
				end_tree_node(app);
			}
		}
		align("\b");
		end_tree_node(obj);
	}
}

void TmoTA_interactable::set_def_types(ObjectTypeSet ots)
{
	set_default_object_type_definitions(ots);
	switch (ots) {
	case OTS_SEPIA: object_type_file_name = "Sepia"; break;
	case OTS_MOT20: object_type_file_name = "MOT20"; break;
	default:object_type_file_name = ""; break;
	}
	post_recreate_gui();
	post_redraw();
}

void TmoTA_interactable::create_gui()
{
	//add_decorator("label tool", "heading");
	if (begin_tree_node("Video", path, true, "level=3")) {
		align("\a");
		add_gui("video_file_name", video_file_name, "file_name",
			"open=true;open_title='open scene file';filter='videos (avi,mpg,mov,mp4,webm):*.avi;*.mpg;*.mov;*.mp4;*.webm;*.3gp|all files:*.*';w=160;save=false;small_icon=true");
		//add_member_control(this, "left_ctrl", left_ctrl, "toggle", "active=false");
		//add_member_control(this, "right_ctrl", right_ctrl, "toggle", "active=false");
		add_view("directory_path", path);
		add_view("json_file_name", json_file_name);
		add_member_control(this, "json_format", object_file_extension, "dropdown", "enums='json,bson,cbor,ubj,bjd'");
		add_view("width", width, "", "w=30", " ");
		add_view("height", height, "", "w=30", " ");
		add_view("nr_frames", nr_read_frames, "", "w=30");
		align("\b");
		end_tree_node(path);
	}
	if (begin_tree_node("Annotation", frame_idx, true, "level=3")) {
		align("\a");
		add_gui("object types", object_type_file_name, "file_name",
			"open=true;open_title='open scene file';filter='csv (scn):*.txt;*.csv|all files:*.*';w=160;save=false;small_icon=true");
		connect_copy(add_button("Sepia", "w=50", " ")->click, cgv::signal::rebind(this, &TmoTA_interactable::set_def_types, cgv::signal::_c<ObjectTypeSet>(OTS_SEPIA)));
		connect_copy(add_button("MOT20", "w=50")->click, cgv::signal::rebind(this, &TmoTA_interactable::set_def_types, cgv::signal::_c<ObjectTypeSet>(OTS_MOT20)));
		add_member_control(this, "frame_idx", frame_idx, "value_slider", "min=0;max=0;ticks=true;tooltip='adjust with@b <left|right|Home|End>'");
		add_member_control(this, "animate", animate, "toggle", "w=30;true_label='@||';false_label='@>';tooltip='toggle playback with@b <Space>'", " ");
		add_member_control(this, "forward", forward, "toggle", "w=30;true_label='@->';false_label='@<-';tooltip='toggle playback direction with@b <D>'", " ");
		add_member_control(this, "restrict", restrict_animation, "toggle", "w=30;label='@<->';tooltip='toggle restriction of playback to appearance with@b <R>'", " ");
		add_member_control(this, "auto_center_view", auto_center_view, "toggle", "w=30;label='@+';tooltip='toggle object centering with@b <C>'", " ");
		add_member_control(this, "focus_current", focus_current, "toggle", "w=30;label='@square';tooltip='toggle object focusing with@b <F>'");
		align("\b");
		end_tree_node(frame_idx);
	}
	/*
	if (begin_tree_node("view", view_scale, false, "level=3")) {
		align("\a");
		add_member_control(this, "scale", view_scale, "value_slider", "min=0.1;max=10;log=true;ticks=true");
		add_member_control(this, "focus_x", view_focus(0), "value_slider", "min=-2;max=2;ticks=true");
		add_member_control(this, "focus_y", view_focus(1), "value_slider", "min=-2;max=2;ticks=true");
		align("\b");
		end_tree_node(mix_factor);
	}
	*/
	if (begin_tree_node("Config", mix_factor, false, "level=3")) {
		align("\a");
		add_decorator("Interaction", "heading", "level=3");
		add_member_control(this, "fps", fps, "value_slider", "min=5;max=300;ticks=true;log=true;tooltip='adjust playback fps with@b <PgDn|PgUp>'");
		add_member_control(this, "span_rectangle_from_center", span_rectangle_from_center, "toggle", "tooltip='whether to span rectangles from center or from other corner'");
		add_member_control(this, "pick_width", pick_width, "value_slider", "min=1;max=50;ticks=true;tooltip='pixel distance for picking rectangle edges'");
		add_member_control(this, "extent_appearance", extent_appearance, "check", "tooltip='whether extension of appearance adds new rectangle or overwrites last'");
		add_decorator("Drawing", "heading", "level=3");
		add_member_control(this, "mix_factor", mix_factor, "value_slider", "min=0;max=1;ticks=true;tooltip='mixing strength of rectangle color (limited to 0.5 in interior)'");
		add_member_control(this, "rectangle_border_width", rectangle_border_width, "value_slider", "min=0;max=10;ticks=true;tooltip='width of opacity ramp at bounding edges'");
		add_member_control(this, "rectangle_border_gamma", rectangle_border_gamma, "value_slider", "min=0.1;max=10;log=true;ticks=true;tooltip='gamma adjustment of opacity ramps'");
		add_member_control(this, "font_color", font_color, "", "tooltip='font color for action tooltip'");
		add_decorator("Layout", "heading", "level=3");
		add_member_control(this, "vertical_viewport_split", vertical_viewport_split, "check", "tooltip='whether to split viewport vertically or horizontally'");
		add_member_control(this, "object_viewport_extent", object_viewport_extent, "value_slider", "min=100;max=500;ticks=true;tooltip='height (horizontal split) or width (vertical split) of object view'");
		add_member_control(this, "object_width_scale", object_width_scale, "value_slider", "min=0;max=1;ticks=true;tooltip='percentual width of object view'");
		add_member_control(this, "panel_border_width", panel_border_width, "value_slider", "min=0;max=10;ticks=true;tooltip='border width in pixels between view panels'");
		align("\b");
		end_tree_node(mix_factor);
	}
	if (begin_tree_node("Objects", objects, true, "level=3")) {
		align("\a");
		create_object_gui_2();
		align("\b");
		end_tree_node(objects);
	}
	configure_controls();
}

bool TmoTA_interactable::find_pick_location(int mouse_x, int mouse_y, vec2& p) const
{
	cgv::render::context* ctx_ptr = get_context();
	if (ctx_ptr) {
		vec3 P = ctx_ptr->get_model_point(mouse_x, ctx_ptr->get_height()-1-mouse_y, MPW);
		if (fabs(P(2)) < 0.1f) {
			p = reinterpret_cast<vec2&>(P);
			return true;
		}
	}
	return false;
}

bool TmoTA_interactable::find_picked_texcoord(int mouse_x, int mouse_y, vec2& tc) const
{
	vec2 p;
	if (!find_pick_location(mouse_x, mouse_y, p))
		return false;

	tc = model_to_texcoord(p);
	return true;
}

bool TmoTA_interactable::find_picked_pixel(int mouse_x, int mouse_y, vec2& pix) const
{
	vec2 tc;
	if (!find_picked_texcoord(mouse_x, mouse_y, tc))
		return false;
	if (allow_subpixel_precision)
		pix = texcoord_to_pixelcoord_f(tc);
	else
		pix = texcoord_to_pixelcoord(tc);
	return true;
}

bool modify_min(int mode)
{
	return (mode & 1) != 0;
}

void TmoTA_interactable::do_ensure_control_value(std::vector<control_value>& video_storage, int frame_idx, float value)
{
	float* value_ptr = 0;
	if (ensure_control_value(video_storage, frame_idx, value, &value_ptr))
		post_recreate_gui();
	else
		update_member(value_ptr);

}

bool TmoTA_interactable::update_location(int mouse_x, int mouse_y)
{
	vec2 pix;
	if (!find_picked_pixel(mouse_x, mouse_y, pix))
		return false;
	vec2 pix_min = pix, pix_max = pix;
	if (rectangle_key != 0) {
		vec2 pix2 = span_rectangle_from_center ? 2.0f * rectangle_pix - pix : rectangle_pix;
		pix_min = vec2(std::min(pix(0), pix2(0)), std::min(pix(1), pix2(1)));
		pix_max = vec2(std::max(pix(0), pix2(0)), std::max(pix(1), pix2(1)));
	}
	if (!objects.empty() && object_idx != -1 && appearance_idx != -1) {
		// determine to be updated appearance
		auto& obj = objects[object_idx];
		int ai = appearance_idx;
		while ((int)frame_idx < obj.appearences[ai].fst_frame_idx && ai > 0)
			--ai;
		while ((int)frame_idx > obj.appearences[ai].lst_frame_idx&& ai + 1 < (int)obj.appearences.size())
			++ai;
		// ensure that appearance contains current frame
		auto& app = obj.appearences[ai];
		if ((int)frame_idx < app.fst_frame_idx) {
			app.fst_frame_idx = frame_idx;
			on_set(&app.fst_frame_idx);
		}
		if ((int)frame_idx > app.lst_frame_idx) {
			app.lst_frame_idx = frame_idx;
			on_set(&app.lst_frame_idx);
		}
		std::vector<control_value>::iterator iter;
		if (over_mode.x_mode == mod_mode(true,true) && over_mode.y_mode == mod_mode(true, true)) {
			float x_min = pix_min(0) + over_mode.rect_offset_x;
			float x_max = x_min + over_mode.rect_width - 1;
			float y_min = pix_min(1) + over_mode.rect_offset_y;
			float y_max = y_min + over_mode.rect_height - 1;
			do_ensure_control_value(app.x_min_values, frame_idx, x_min);
			do_ensure_control_value(app.x_max_values, frame_idx, x_max);
			do_ensure_control_value(app.y_min_values, frame_idx, y_min);
			do_ensure_control_value(app.y_max_values, frame_idx, y_max);
		}
		else {
			if (modify_min(over_mode.x_mode) || rectangle_key != 0) {
				float ub;
				extract_value(app.x_max_values, frame_idx, ub);
				if (pix_min(0) > ub)
					pix_min(0) = ub;
				do_ensure_control_value(app.x_min_values, frame_idx, pix_min(0));
			}
			if (modify_max(over_mode.x_mode) || rectangle_key != 0) {
				float lb;
				extract_value(app.x_min_values, frame_idx, lb);
				if (pix_max(0) < lb)
					pix_max(0) = lb;
				do_ensure_control_value(app.x_max_values, frame_idx, pix_max(0));
			}
			if (modify_min(over_mode.y_mode) || rectangle_key != 0) {
				float ub;
				extract_value(app.y_max_values, frame_idx, ub);
				if (pix_min(1) > ub)
					pix_min(1) = ub;
				do_ensure_control_value(app.y_min_values, frame_idx, pix_min(1));
			}
			if (modify_max(over_mode.y_mode) || rectangle_key != 0) {
				float lb;
				extract_value(app.y_min_values, frame_idx, lb);
				if (pix_max(1) < lb)
					pix_max(1) = lb;
				do_ensure_control_value(app.y_max_values, frame_idx, pix_max(1));
			}
		}
	}
	post_redraw();
	return true;
}
int TmoTA_interactable::add_appearance(tracked_object& obj, int frame_idx, bool select)
{
	int ai = (int)obj.appearences.size();
	open_appearance_object_idx = (int)(&obj - &objects.front());
	open_appearance_idx = ai;
	obj.appearences.push_back(appearance());
	auto& app = obj.appearences[ai];
	app.fst_frame_idx = frame_idx;
	app.lst_frame_idx = lst_frame_idx();
	on_set(&app.lst_frame_idx);
	if (select) {
		appearance_idx = ai;
		on_set(&appearance_idx);
	}
	return ai;
}

void TmoTA_interactable::append_rectangle(appearance& app, int x, int y, int frame_idx)
{
	vec2 pix;
	if (find_picked_pixel(last_x, last_y, pix)) {
		app.append_rectangle(vec4(pix(0),pix(1),pix(0),pix(1)), frame_idx);
		post_recreate_gui();
	}
}
int TmoTA_interactable::add_appearance(tracked_object& obj, int frame_idx, int x, int y, bool select)
{
	int ai = add_appearance(obj, frame_idx, select);
	append_rectangle(obj.appearences[ai], x, y, frame_idx);
	return ai;
}
int TmoTA_interactable::add_object(ObjectType ot, const std::string& type_name, bool select)
{
	int oi = int(objects.size());
	objects.push_back(tracked_object(ot, type_name));
	add_appearance(objects[oi], frame_idx, select);
	if (select) {
		object_idx = oi;
		on_set(&object_idx);
	}
	return oi;
}
int TmoTA_interactable::add_object(ObjectType ot, const std::string& type_name, int x, int y, bool select)
{
	int oi = int(objects.size());
	objects.push_back(tracked_object(ot, type_name));
	add_appearance(objects[oi], frame_idx, x, y, select);
	if (select) {
		object_idx = oi;
		on_set(&object_idx);
	}
	return oi;
}

bool TmoTA_interactable::handle_modifier_key(int mx, int my, cgv::gui::KeyAction ka)
{
	switch (ka) {
	case cgv::gui::KA_PRESS:
		x_mode = mx;
		y_mode = my;
//		dynamic_cast<cgv::base::base*>(get_context())->set("cursor", "nwse");
		on_set(&x_mode);
		on_set(&y_mode);
		update_location(last_x, last_y);
		post_redraw();
		return true;
	case cgv::gui::KA_RELEASE:
		x_mode = mod_mode(false, false);
		y_mode = mod_mode(false, false);
//		dynamic_cast<cgv::base::base*>(get_context())->set("cursor", "default");
		on_set(&x_mode);
		on_set(&y_mode);
		post_redraw();
		return true;
	}
	return false;
}

bool TmoTA_interactable::handle_rectangle_modifier_key(cgv::gui::KeyAction ka, char key)
{
	switch (ka) {
	case cgv::gui::KA_PRESS:
	{
		vec2 pix;
		if (rectangle_key == 0 && find_picked_pixel(last_x, last_y, pix)) {
			switch (key) {
			case 'O':
			case 'o':
				add_object(OT_CAR, get_object_type_name(OT_CAR), last_x, last_y, true);
				restrict_animation = true;
				update_member(&restrict_animation);
				break;
			case 'A':
			case 'a':
				if (!objects.empty() && object_idx != -1) {
					auto& obj = objects[object_idx];
					add_appearance(obj, frame_idx, last_x, last_y, true);
					restrict_animation = true;
					update_member(&restrict_animation);
				}
				else
					return true;
				break;
			case 'V':
			case 'v':
				if (!objects.empty() && object_idx != -1 && appearance_idx != -1) {
					auto& obj = objects[object_idx];
					auto& app = obj.appearences[appearance_idx];
					open_appearance_object_idx = -1;
					open_appearance_idx = -1;
					app.lst_frame_idx = frame_idx;
					on_set(&app.lst_frame_idx);
					append_rectangle(app, last_x, last_y, frame_idx);
				}
				else
					return true;
				break;
			}
			rectangle_pix = pix;
			rectangle_key = key;
			on_set(&rectangle_key);

			update_location(last_x, last_y);
		}
		return true;
	}
	case cgv::gui::KA_RELEASE:
		if (key == rectangle_key) {
			update_location(last_x, last_y);
			rectangle_key = 0;
			on_set(&rectangle_key);
		}
		return true;
	}
	return true;
}

bool TmoTA_interactable::select_frame(int x)
{
	float x_t = (float(x - object_viewport[0]) / object_viewport[2] * 2 - 1.0f);
	if (fabs(x_t) > std::min(1.0f,object_width_scale*1.02f))
		return false;
	frame_idx = std::min(
		lst_frame_idx(), 
		std::max(
			fst_frame_idx, 
			int(((int)nr_frames - 1) * (x_t + object_width_scale) / (2* object_width_scale) + fst_frame_idx)
		)
	);
	on_set(&frame_idx);
	return true;
}

int TmoTA_interactable::determine_mode(int x, int y, int object_idx, int frame_idx, int& x_mode, int& y_mode, int* x_off_ptr, int* y_off_ptr, int* width_ptr, int* height_ptr) const
{
	const auto& obj = objects[object_idx];
	const appearance* app_ptr = 0;
	size_t ai;
	for (ai = 0; ai < obj.appearences.size(); ++ai) {
		const auto& app = obj.appearences[ai];
		if ((int)frame_idx < app.fst_frame_idx)
			break;
		if ((int)frame_idx >= app.fst_frame_idx && (int)frame_idx <= app.lst_frame_idx) {
			app_ptr = &app;
			break;
		}
	}
	if (!app_ptr)
		return -(int)ai;
	vec4 p;
	if (extract_value(app_ptr->x_min_values, frame_idx, p[0]) &&
		extract_value(app_ptr->y_min_values, frame_idx, p[1]) &&
		extract_value(app_ptr->x_max_values, frame_idx, p[2]) &&
		extract_value(app_ptr->y_max_values, frame_idx, p[3])) {
		vec2 pix;
		if (width_ptr)
			*width_ptr = (int)p[2] - (int)p[0] + 1;
		if (height_ptr)
			*height_ptr = (int)p[3] - (int)p[1] + 1;

		x_mode = y_mode = mod_mode(false, false);
		if (find_picked_pixel(x, y, pix)) {
			if (x_off_ptr)
				*x_off_ptr = int(p[0] - pix(0));
			if (y_off_ptr)
				*y_off_ptr = int(p[1] - pix(1));
			float Lb = p[0] - pick_width;
			float Ub = p[2] + pick_width;
			int d = std::min(pick_width,(int)std::max(0.0f, p[2] - p[0] + 1 - pick_width) / 2);
			float lb = p[0] + d;
			float ub = p[2] - d;
			int x_loc = -1;
			if (pix(0) >= Lb && pix(0) <= Ub) {
				if (pix(0) < lb)
					x_loc = 1;
				else if (pix(0) < ub)
					x_loc = 0;
				else
					x_loc = 2;
			}
			Lb = (float)p[1] - pick_width;
			Ub = (float)p[3] + pick_width;
			d = std::min(pick_width,(int)std::max(0.0f, p[3] - p[1] + 1 - pick_width) / 2);
			lb = (float)p[1] + d;
			ub = (float)p[3] - d;
			int y_loc = -1;
			if (pix(1) >= Lb && pix(1) <= Ub) {
				if (pix(1) < lb)
					y_loc = 1;
				else if (pix(1) < ub)
					y_loc = 0;
				else
					y_loc = 2;
			}
			if (x_loc == -1 || y_loc == -1)
				x_mode = y_mode = 0;
			else if (x_loc == 0 && y_loc == 0)
				x_mode = y_mode = 3;
			else {
				x_mode = x_loc;
				y_mode = y_loc;
			}
			return 3;
		}
		return 2;
	}
	else 
		return 1;
}

std::vector<int> TmoTA_interactable::find_objects(int mx, int my) const
{
	std::vector<int> obj_idxs;
	vec2 pix;
	if (!find_picked_pixel(mx, my, pix))
		return obj_idxs;
	size_t oi;
	for (oi = 0; oi < objects.size(); ++oi) {
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
				if (!allow_subpixel_precision) {
					p[0] = floor(p[0]);
					p[1] = floor(p[1]);
					p[2] = floor(p[2]) + 1.0f;
					p[3] = floor(p[3]) + 1.0f;
				}
				if (pix(0) >= p[0] && pix(0) <= p[2] && pix(1) >= p[1] && pix(1) <= p[3]) {
					obj_idxs.push_back(int(oi));
					//std::cout << "found " << oi << std::endl;
				}
			}
		}
	}
	return obj_idxs;
}

void show_over_mode(const mouse_over_mode& over_mode, char rect_key)
{
	std::cout << "<" << over_mode.mode << "|" << (rect_key == 0 ? ' ' : rect_key) << "> " << over_mode.x_mode << "," << over_mode.y_mode
		<< " [" << over_mode.rect_offset_x << "," << over_mode.rect_offset_y << ":" << over_mode.rect_width << "x" << over_mode.rect_height << "] "
		<< over_mode.new_object_idx << "|" << over_mode.nr_objects << std::endl;
}

mouse_over_mode TmoTA_interactable::determine_over_mode(int x, int y) const
{
	mouse_over_mode over_mode;
	over_mode.mouse_x = x;
	over_mode.mouse_y = y;
	over_mode.x_mode = mod_mode(false, false);
	over_mode.y_mode = mod_mode(false, false);
	over_mode.mode = 0;
	if (object_idx != -1) {
		over_mode.mode = determine_mode(x, y, object_idx, frame_idx, over_mode.x_mode, over_mode.y_mode,
			&over_mode.rect_offset_x, &over_mode.rect_offset_y, &over_mode.rect_width, &over_mode.rect_height);
	}
	if (object_idx == -1 || over_mode.mode != 3 || (over_mode.x_mode == mod_mode(false, false) && over_mode.y_mode == mod_mode(false, false))) {
		if (!focus_current) {
			std::vector<int> obj_idxs = find_objects(x, y);
			over_mode.nr_objects = (int)obj_idxs.size();
			over_mode.new_object_idx = obj_idxs.empty() ? -1 : obj_idxs[obj_layer % obj_idxs.size()];
		}
	}
	//show_over_mode(over_mode, rectangle_key);
	return over_mode;
}

void TmoTA_interactable::update_over_mode(int x, int y, bool in_video_viewport)
{
	mouse_over_mode new_over_mode;
	if (in_video_viewport)
		new_over_mode = determine_over_mode(x, y);
	new_over_mode.in_video_viewport = in_video_viewport;
	new_over_mode.connect_left = left_ctrl;
	new_over_mode.connect_right = right_ctrl;
	if (over_mode.new_object_idx != new_over_mode.new_object_idx ||
		over_mode.x_mode != new_over_mode.x_mode ||
		over_mode.y_mode != new_over_mode.y_mode ||
		over_mode.connect_left != new_over_mode.connect_left ||
		over_mode.connect_right != new_over_mode.connect_right ||
		((over_mode.nr_objects > 0 || over_mode.mode <= 0) && (over_mode.mouse_x != new_over_mode.mouse_x || over_mode.mouse_y != new_over_mode.mouse_y)))
		post_redraw();
	over_mode = new_over_mode;
}

void TmoTA_interactable::offset_appearance(appearance& app, int start_frame_idx)
{
	if (app.fst_frame_idx >= start_frame_idx || start_frame_idx >= app.lst_frame_idx)
		return;
	offset_values(app.x_min_values, start_frame_idx);
	offset_values(app.x_max_values, start_frame_idx);
	offset_values(app.y_min_values, start_frame_idx);
	offset_values(app.y_max_values, start_frame_idx);
	app.fst_frame_idx = start_frame_idx;

}

void TmoTA_interactable::truncate_appearance(appearance& app, int end_frame_idx)
{
	if (app.lst_frame_idx <= end_frame_idx || end_frame_idx <= app.fst_frame_idx)
		return;
	truncate_values(app.x_min_values, end_frame_idx);
	truncate_values(app.x_max_values, end_frame_idx);
	truncate_values(app.y_min_values, end_frame_idx);
	truncate_values(app.y_max_values, end_frame_idx);
	app.lst_frame_idx = end_frame_idx;
}

bool TmoTA_interactable::handle(cgv::gui::event& e)
{
	if (e.get_kind() == cgv::gui::EID_MOUSE) {
		cgv::gui::mouse_event& me = static_cast<cgv::gui::mouse_event&>(e);
		// remember mouse position
		last_x = me.get_x();
		last_y = me.get_y();
		// context and viewport needs to be available
		cgv::render::context* ctx_ptr = get_context();
		if (!ctx_ptr)
			return false;
		// check into which viewport mouse points
		int x = last_x, y = ctx_ptr->get_height() - me.get_y();
		bool in_video_viewport =
			x >= video_viewport[0] && x < video_viewport[0] + video_viewport[2] &&
			y >= video_viewport[1] && y < video_viewport[1] + video_viewport[3];
		bool in_object_viewport =
			x >= object_viewport[0] && x < object_viewport[0] + object_viewport[2] &&
			y >= object_viewport[1] && y < object_viewport[1] + object_viewport[3];
		// switch over action of mouse event
		switch (me.get_action()) {
		case cgv::gui::MA_MOVE:
#ifdef DEBUG_EVENTS
			std::cout << "move" << std::endl;
#endif
			potential_left_click = false;
			potential_right_click = false;
			if (rectangle_key != 0 || x_mode != mod_mode(false, false) || y_mode != mod_mode(false, false)) {
				update_location(me.get_x(), me.get_y());
				std::cout << "update (" << rectangle_key << ":" << me.get_x() << "," << me.get_y() << ")" << std::endl;
				return true;
			}
			else {
				//int new_x_over_mode = mod_mode(false, false);
				//int new_y_over_mode = mod_mode(false, false);
				//if (in_video_viewport) {
					//std::cout << "MOVE" << std::endl;
					update_over_mode(me.get_x(), me.get_y(), in_video_viewport);
					//if (object_idx == -1 ||
					//    3 != determine_mode(me.get_x(), me.get_y(), object_idx, frame_idx, new_x_over_mode, new_y_over_mode) ||
					//	(new_x_over_mode == mod_mode(false, false) && new_y_over_mode == mod_mode(false, false))) {
					//	std::vector<int> obj_idxs = find_objects(me.get_x(), me.get_y());
					//	new_object_idx = obj_idxs.empty() ? -1 : obj_idxs[obj_layer % obj_idxs.size()];
					//	//std::cout << new_object_idx << std::endl;
					//}
				//}
				//if (new_x_over_mode != x_over_mode || new_y_over_mode != y_over_mode) {
				//	x_over_mode = new_x_over_mode; 
				//	y_over_mode = new_y_over_mode;
				//	post_redraw();
				//}
			}
			break;
		case cgv::gui::MA_PRESS:
#ifdef DEBUG_EVENTS
			std::cout << "press" << std::endl;
#endif
			if (me.get_button() == cgv::gui::MB_RIGHT_BUTTON) {
				potential_right_click = true;
				right_clicked = true;
				pressed_in_object_viewport = in_object_viewport;
				pressed_in_video_viewport = in_video_viewport;
			}
			if (me.get_button() == cgv::gui::MB_LEFT_BUTTON) {
				left_clicked = true;
				potential_left_click = true;
				if (in_object_viewport)
					select_frame(me.get_x());
				pressed_in_object_viewport = in_object_viewport;
				pressed_in_video_viewport = in_video_viewport;
				//std::cout << "PRESS" << std::endl;
				if (in_video_viewport && object_idx != -1) {
					if (over_mode.mode == 3 &&
						(over_mode.x_mode != mod_mode(false, false) || over_mode.y_mode != mod_mode(false, false))) {
						update_location(me.get_x(), me.get_y());
						potential_left_click = false;
					}
					else {
						if (over_mode.mode <= 0) {
							if (over_mode.connect_right && over_mode.mode < 0) {
								appearance_idx = -over_mode.mode - 1;
								auto& app = objects[object_idx].appearences[appearance_idx];
								app.lst_frame_idx = frame_idx;
								vec4 r = app.last_rectangle();
								over_mode.mode = 3;
								over_mode.x_mode = mod_mode(true, true);
								over_mode.y_mode = mod_mode(true, true);
								over_mode.rect_width = (int)(r(2)-r(0) + 1);
								over_mode.rect_height = (int)(r(3)-r(1) + 1);
								over_mode.rect_offset_x = -(int)(0.5f * over_mode.rect_width);
								over_mode.rect_offset_y = -(int)(0.5f * over_mode.rect_height);
								vec2 pix;
								if (find_picked_pixel(me.get_x(), me.get_y(), pix)) {
									vec2 off = pix - 0.5f * vec2(r(0)+r(2), r(1)+r(3));
									r += vec4(off(0), off(1), off(0), off(1));
								}
								if (extent_appearance)
									app.append_rectangle(r, frame_idx);
								else
									app.set_last_rectangle(r, frame_idx);
								on_set(&appearance_idx);
								post_recreate_gui();
								potential_left_click = false;
							}
							else if (over_mode.connect_left && -over_mode.mode < objects[object_idx].appearences.size()) {
								appearance_idx = -over_mode.mode;
								auto& app = objects[object_idx].appearences[appearance_idx];
								app.fst_frame_idx = frame_idx;
								vec4 r = app.first_rectangle();
								over_mode.mode = 3;
								over_mode.x_mode = mod_mode(true, true);
								over_mode.y_mode = mod_mode(true, true);
								over_mode.rect_width =  (int)(app.x_max_values.front().value - app.x_min_values.front().value + 1);
								over_mode.rect_height = (int)(app.y_max_values.front().value - app.y_min_values.front().value + 1);
								over_mode.rect_offset_x = -(int)(0.5f * over_mode.rect_width);
								over_mode.rect_offset_y = -(int)(0.5f * over_mode.rect_height);
								vec2 pix;
								if (find_picked_pixel(me.get_x(), me.get_y(), pix)) {
									vec2 off = pix - 0.5f * vec2(r(0) + r(2), r(1) + r(3));
									r += vec4(off(0), off(1), off(0), off(1));
								}
								if (extent_appearance)
									app.prepend_rectangle(r, frame_idx);
								else
									app.set_first_rectangle(r, frame_idx);
								on_set(&appearance_idx);
								post_recreate_gui();
								potential_left_click = false;
							}
							else if (objects[object_idx].appearences.size() > 0 && (int)frame_idx > objects[object_idx].appearences.back().lst_frame_idx)
								handle_rectangle_modifier_key(cgv::gui::KA_PRESS, 'a');
							else
								handle_rectangle_modifier_key(cgv::gui::KA_PRESS, 'o');
						}
						else if (over_mode.mode == 1)
							handle_rectangle_modifier_key(cgv::gui::KA_PRESS, 'v');
						else {
							//std::vector<int> obj_idxs = find_objects(me.get_x(), me.get_y());
							if (over_mode.new_object_idx == -1) {
								object_idx = over_mode.new_object_idx;
								on_set(&object_idx);
								if (object_idx != -1)
									on_set(&frame_idx);
								potential_left_click = false;
							}
						}
					}
					// find appearance
					//bool found = false;
					//int xo = 0, yo = 0, w = 1, h = 1;
					//int mode = determine_mode(me.get_x(), me.get_y(), object_idx, frame_idx, x_mode, y_mode, &xo, &yo, &w, &h);
					//if (mode == 3 && (x_mode != mod_mode(false, false) || y_mode != mod_mode(false, false))) {
					//	update_member(&x_mode);
					//	update_member(&y_mode);
					//	if (x_mode == mod_mode(true, true) && y_mode == mod_mode(true, true)) {
					//		rect_width = w;
					//		rect_height = h;
					//	}
					//	update_location(me.get_x(), me.get_y());
					//	potential_left_click = false;
					//}
					//else {
					//	if (mode == 0 && objects[object_idx].appearences.size() > 0 && (int)frame_idx > objects[object_idx].appearences.back().lst_frame_idx)
					//		handle_rectangle_modifier_key(cgv::gui::KA_PRESS, 'a');
					//	else if (mode == 1)
					//		handle_rectangle_modifier_key(cgv::gui::KA_PRESS, 'v');
					//	else {
					//		std::vector<int> obj_idxs = find_objects(me.get_x(), me.get_y());
					//		if (obj_idxs.empty()) {
					//			object_idx = -1;
					//			on_set(&object_idx);
					//			potential_left_click = false;
					//		}
					//		else {
					//			object_idx = obj_idxs[obj_layer % obj_idxs.size()];
					//			on_set(&object_idx);
					//			on_set(&frame_idx);
					//			potential_left_click = false;
					//		}
					//	}
					//}
				}
			}
			return true;
		case cgv::gui::MA_RELEASE:
#ifdef DEBUG_EVENTS
			std::cout << "release" << std::endl;
#endif
			if (me.get_button() == cgv::gui::MB_RIGHT_BUTTON) {
				if (in_video_viewport && potential_right_click) {
					vec2 p;
					if (find_pick_location(me.get_x(), me.get_y(), p)) {
						cgv::gui::animate_with_linear_blend(view_focus(0), p(0), 0.4)->set_base_ptr(this);
						cgv::gui::animate_with_linear_blend(view_focus(1), p(1), 0.4)->set_base_ptr(this);
					}
				}
				right_clicked = false;
				potential_right_click = false;
			}
			if (me.get_button() == cgv::gui::MB_LEFT_BUTTON) {
				left_clicked = false;
				//std::cout << "RELEASE" << std::endl;
				if (rectangle_key == 'o' || rectangle_key == 'v' || rectangle_key == 'a') {
					handle_rectangle_modifier_key(cgv::gui::KA_RELEASE, rectangle_key);
					update_over_mode(me.get_x(), me.get_y(), in_video_viewport);
					return true;
				}
				else if (over_mode.x_mode != mod_mode(false, false) || over_mode.y_mode != mod_mode(false, false)) {
					over_mode.x_mode = mod_mode(false, false);
					over_mode.y_mode = mod_mode(false, false);
				}
				if (potential_left_click) {
					if (pressed_in_video_viewport) {
						pressed_in_video_viewport = false;
						object_idx = over_mode.new_object_idx;
						on_set(&object_idx);
						if (object_idx != -1)
							on_set(&frame_idx);
					}
					if (pressed_in_object_viewport) {
						pressed_in_object_viewport = false;
						if (select_frame(me.get_x())) {
							if (objects.size() > 0) {
								float of = (1.0f - float(ctx_ptr->get_height() - me.get_y() - object_viewport[1]) / object_viewport[3]) * objects.size();
								float oo = floor(of) + 0.5f;
								if (fabs(of - oo) < 0.3f)
									object_idx = int(floor(of));
								else
									object_idx = -1;
								on_set(&object_idx);
								select_frame(me.get_x());
							}
						}
					}
				}
				potential_left_click = false;

				//else if (x_mode != mod_mode(false, false) || y_mode != mod_mode(false, false)) {
				//	x_mode = mod_mode(false, false);
				//	y_mode = mod_mode(false, false);
				//	update_member(&x_mode);
				//	update_member(&y_mode);
				//}
				//if (potential_left_click) {
				//	if (pressed_in_video_viewport) {
				//		pressed_in_video_viewport = false;
				//		std::vector<int> obj_idxs = find_objects(me.get_x(), me.get_y());
				//		object_idx = obj_idxs.empty() ? -1 : obj_idxs[obj_layer % obj_idxs.size()];
				//		on_set(&object_idx);
				//		if (object_idx != -1)
				//			on_set(&frame_idx);
				//	}
				//	if (pressed_in_object_viewport) {
				//		pressed_in_object_viewport = false;
				//		if (select_frame(me.get_x())) {
				//			if (objects.size() > 0) {
				//				float of = (1.0f - float(ctx_ptr->get_height() - me.get_y() - object_viewport[1]) / object_viewport[3]) * objects.size();
				//				float oo = floor(of) + 0.5f;
				//				if (fabs(of - oo) < 0.3f)
				//					object_idx = int(floor(of));
				//				else
				//					object_idx = -1;
				//				on_set(&object_idx);
				//				select_frame(me.get_x());
				//			}
				//		}
				//	}
				//}
				//potential_left_click = false;
			}
			return true;
		case cgv::gui::MA_WHEEL:
			if (in_video_viewport) {
				if (me.get_modifiers() == cgv::gui::EM_SHIFT || me.get_modifiers() == cgv::gui::EM_CTRL) {
					obj_layer += me.get_dy();
					update_over_mode(me.get_x(), me.get_y(), over_mode.in_video_viewport);
				}
				else {
					vec2 p;
					float factor = exp(-0.2f * float(me.get_dy()));
					if (find_pick_location(me.get_x(), me.get_y(), p)) {
						view_focus = p + 1.0f / factor * (view_focus - p);
						on_set(&view_focus(0));
						on_set(&view_focus(1));
					}
					view_scale *= factor;
					on_set(&view_scale);
				}
			}
			return true;
		case cgv::gui::MA_DRAG:
#ifdef DEBUG_EVENTS
			std::cout << "drag" << std::endl;
#endif
			potential_right_click = false;
			potential_left_click = false;
			if (pressed_in_video_viewport && me.get_button_state() == cgv::gui::MB_LEFT_BUTTON) {
				//std::cout << "DRAG" << std::endl;
				if (rectangle_key != 'o' && rectangle_key != 'v' && rectangle_key != 'a') {
					if (over_mode.x_mode != mod_mode(false, false) || over_mode.y_mode != mod_mode(false, false)) {
						update_location(me.get_x(), me.get_y());
					}
					else if (object_idx == -1) {
						handle_rectangle_modifier_key(cgv::gui::KA_PRESS, 'o');
					}
				}
				else {
					update_location(me.get_x(), me.get_y());
				}
			}
			if (pressed_in_video_viewport && me.get_button_state() == cgv::gui::MB_RIGHT_BUTTON) {
				view_focus(0) -= 2.0f * video_aspect * float(me.get_dx()) / (view_scale * video_viewport[2]);
				view_focus(1) += 2.0f * float(me.get_dy()) / (view_scale * video_viewport[3]);
				on_set(&view_focus(0));
				on_set(&view_focus(1));
				return true;
			}
			if (pressed_in_object_viewport && me.get_button_state() == cgv::gui::MB_LEFT_BUTTON)
				select_frame(me.get_x());
			break;
			case cgv::gui::MA_ENTER:
				update_over_mode(me.get_x(), me.get_y(), in_video_viewport);
#ifdef DEBUG_EVENTS
				std::cout << "enter" << std::endl;
#endif
				break;
			case cgv::gui::MA_LEAVE:
				update_over_mode(me.get_x(), me.get_y(), false);
#ifdef DEBUG_EVENTS
				std::cout << "leave" << std::endl;
#endif
				break;
		}
		return false;
	}
	if (e.get_kind() != cgv::gui::EID_KEY)
		return false;
	cgv::gui::key_event& ke = static_cast<cgv::gui::key_event&>(e);

	//switch (ke.get_key()) {
	//case cgv::gui::KEY_Num_1: return handle_modifier_key(mod_mode(true , false), mod_mode(false, true ), ke.get_action());
	//case cgv::gui::KEY_Num_2: return handle_modifier_key(mod_mode(false, false), mod_mode(false, true), ke.get_action());
	//case cgv::gui::KEY_Num_3: return handle_modifier_key(mod_mode(false, true), mod_mode(false, true), ke.get_action());
	//case cgv::gui::KEY_Num_4: return handle_modifier_key(mod_mode(true, false), mod_mode(false, false), ke.get_action());
	//case cgv::gui::KEY_Num_6: return handle_modifier_key(mod_mode(false, true), mod_mode(false, false), ke.get_action());
	//case cgv::gui::KEY_Num_7: return handle_modifier_key(mod_mode(true, false), mod_mode(true, false), ke.get_action());
	//case cgv::gui::KEY_Num_8: return handle_modifier_key(mod_mode(false, false), mod_mode(true, false), ke.get_action());
	//case cgv::gui::KEY_Num_9: return handle_modifier_key(mod_mode(false, true), mod_mode(true, false), ke.get_action());
	//case cgv::gui::KEY_Num_5: return handle_rectangle_modifier_key(ke.get_action(), 'C');
	//}
	if (ke.get_action() == cgv::gui::KA_RELEASE) {
		switch (ke.get_key()) {
		case cgv::gui::KEY_Left_Ctrl:
			left_ctrl = false;
			update_over_mode(over_mode.mouse_x, over_mode.mouse_y, over_mode.in_video_viewport);
			update_member(&left_ctrl);
			return false;
		case cgv::gui::KEY_Right_Ctrl:
			right_ctrl = false;
			update_over_mode(over_mode.mouse_x, over_mode.mouse_y, over_mode.in_video_viewport);
			update_member(&right_ctrl);
			return false;
		}
		return false;
	}
	switch (ke.get_key()) {
	case cgv::gui::KEY_Left_Ctrl:
		if (ke.get_action() == cgv::gui::KA_PRESS) {
			left_ctrl = true;
			update_over_mode(over_mode.mouse_x, over_mode.mouse_y, over_mode.in_video_viewport);
			update_member(&left_ctrl);
		}
		return false;
	case cgv::gui::KEY_Right_Ctrl:
		if (ke.get_action() == cgv::gui::KA_PRESS) {
			right_ctrl = true;
			update_over_mode(over_mode.mouse_x, over_mode.mouse_y, over_mode.in_video_viewport);
			update_member(&right_ctrl);
		}
		return false;
	case cgv::gui::KEY_Right:
		if ((int)frame_idx < lst_frame_idx()) {
			frame_idx = std::min((unsigned)lst_frame_idx(), frame_idx + (ke.get_modifiers() == cgv::gui::EM_SHIFT ? 10 : 1));
			if (!forward) {
				forward = true;
				update_member(&forward);
			}
			on_set(&frame_idx);
		}
		return true;
	case cgv::gui::KEY_Left:
		if (frame_idx > fst_frame_idx) {
			if (forward) {
				forward = false;
				update_member(&forward);
			}
			frame_idx = std::max(fst_frame_idx, (int)frame_idx - (ke.get_modifiers() == cgv::gui::EM_SHIFT ? 10 : 1));
			on_set(&frame_idx);
		}
		return true;
	case cgv::gui::KEY_Escape:
	case cgv::gui::KEY_Enter:
		if (object_idx != -1) {
			object_idx = -1;
			on_set(&object_idx);
		}
		return true;
	case cgv::gui::KEY_Home:
		if (restrict_animation && !objects.empty() && object_idx != -1 && appearance_idx != -1) {
			int fst_idx = objects[object_idx].appearences[appearance_idx].fst_frame_idx;
			while ((int)frame_idx <= fst_idx && appearance_idx > 0) {
				--appearance_idx;
				fst_idx = objects[object_idx].appearences[appearance_idx].fst_frame_idx;
			}
			frame_idx = fst_idx;
		}
		else
			frame_idx = fst_frame_idx;
		on_set(&frame_idx);
		return true;
	case cgv::gui::KEY_End:
		if (restrict_animation && !objects.empty() && object_idx != -1 && appearance_idx != -1) {
			int lst_idx = objects[object_idx].appearences[appearance_idx].lst_frame_idx;
			while ((int)frame_idx >= lst_idx && appearance_idx + 1 < objects[object_idx].appearences.size()) {
				++appearance_idx;
				lst_idx = objects[object_idx].appearences[appearance_idx].lst_frame_idx;
			}
			frame_idx = lst_idx;
		}
		else
			frame_idx = lst_frame_idx();
		on_set(&frame_idx);
		return true;
	case 'F':
		focus_current = !focus_current;
		on_set(&focus_current);
		return true;
	case 'W':
	case cgv::gui::KEY_Page_Up:
		fps *= 1.1f;
		if (fps > 300)
			fps = 300;
		on_set(&fps);
		return true;
	case 'X':
	case cgv::gui::KEY_Page_Down:
		fps /= 1.1f;
		if (fps < 5)
			fps = 5;
		on_set(&fps);
		return true;
	case cgv::gui::KEY_Back_Space:
		if (object_idx != -1) {
			auto& apps = objects[object_idx].appearences;
			if (appearance_idx != -1) {
				if (ke.get_modifiers() == 0) {
					if (!(open_appearance_object_idx == object_idx && open_appearance_idx == appearance_idx)) {
						offset_appearance(apps[appearance_idx], frame_idx);
						post_recreate_gui();
					}
				}
			}
			post_redraw();
			update_over_mode(over_mode.mouse_x, over_mode.mouse_y, over_mode.in_video_viewport);
			return true;
		}
		break;
	case cgv::gui::KEY_Delete:
		if (object_idx != -1) {
			auto& apps = objects[object_idx].appearences;
			if (appearance_idx != -1) {
				if (ke.get_modifiers() == cgv::gui::EM_ALT) {
					apps.erase(apps.begin() + appearance_idx);
					if (open_appearance_object_idx == object_idx && open_appearance_idx > appearance_idx)
						--open_appearance_idx;
					--appearance_idx;
					on_set(&appearance_idx);
					post_recreate_gui();
				}
				if (ke.get_modifiers() == 0) {
					if (!(open_appearance_object_idx == object_idx && open_appearance_idx == appearance_idx)) {
						truncate_appearance(apps[appearance_idx], frame_idx);
						post_recreate_gui();
					}
				}
			}
			if (apps.empty() || ke.get_modifiers() == cgv::gui::EM_CTRL) {
				objects.erase(objects.begin() + object_idx);
				if (open_appearance_object_idx != -1 && open_appearance_object_idx == object_idx) {
					open_appearance_object_idx = -1;
					open_appearance_idx = -1;
				}
				--object_idx;
				on_set(&object_idx);
				post_recreate_gui();
			}
			post_redraw();
			update_over_mode(over_mode.mouse_x, over_mode.mouse_y, over_mode.in_video_viewport);
			return true;
		}
		break;
	case cgv::gui::KEY_Space:
		animate = !animate;
		if (animate)
			last_t = trig.get_current_time();
		update_member(&animate);
		return true;
	case 'R':
		restrict_animation = !restrict_animation;
		update_member(&restrict_animation);
		return true;
	case 'C':
		auto_center_view = !auto_center_view;
		if (auto_center_view)
			ensure_centered_view();
		on_set(&auto_center_view);
		return true;
	case 'D':
		forward = !forward;
		on_set(&forward);
		return true;
	case cgv::gui::KEY_F2:
		if (!save_object_file(get_json_file_name()))
			cgv::gui::message("could not save file");
		else
			color_impuls(rgb(0, 1, 0));
		return true;
	case cgv::gui::KEY_F3:
	{
		std::string csv_file_name = json_file_name + "_MOT.txt";
		if (!save_MOT_file(csv_file_name))
			cgv::gui::message("could not save file");
		else
			color_impuls(rgb(0, 1, 0));
		return true; 
	}
	case cgv::gui::KEY_Down:
		if (object_idx + 1 < (int)objects.size())
			++object_idx;
		else
			object_idx = -1;
		on_set(&object_idx);
		return true;
	case cgv::gui::KEY_Up:
		if (object_idx == -1)
			object_idx = (int)objects.size() - 1;
		else
			--object_idx;
		on_set(&object_idx);
		return true;
	}
	return false;
}

void TmoTA_interactable::stream_help(std::ostream& os)
{
	os << "label_tool: navigation keys";
}
void TmoTA_interactable::stream_stats(std::ostream& os)
{
	os << "label_tool: x_mode=" << over_mode.x_mode << ", y_mode=" << over_mode.y_mode;
}

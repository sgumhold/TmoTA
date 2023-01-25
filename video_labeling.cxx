#include "video_labeling.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cgv/utils/file.h>
#include <cgv/gui/dialog.h>

using json = nlohmann::json;

//size_t get_object_type_begin()
//{
//	return 1;
//}
//

size_t get_nr_object_types(ObjectTypeSet ots)
{
	static size_t ots_sizes[] = { OT_END, 14 };
	return ots_sizes[ots];
}

/// helper function to transform an object type into a string
std::string get_Sepia_object_type_name(ObjectType ot)
{
	const char* object_type_names[] = {
		"user defined",
		"car",
		"truck",
		"bus",
		"bicycle",
		"motorcycle",
		"trailer",
		"tram",
		"train",
		"caravan",
		"agricultural vehicle",
		"construction vehicle",
		"emergency vehicle",
		"passive vehicle",
		"person",
		"large animal",
		"small animal"
	};
	return object_type_names[ot];
}
std::string get_MOT20_object_type_name(ObjectType ot)
{
	const char* object_type_names[] = {
		"user defined",
		"Pedestrian",
		"Person on vehicle",
		"Car",
		"Bicycle",
		"Motorbike",
		"Non motorized vehicle",
		"Static person",
		"Distractor",
		"Occluder",
		"Occluder on the ground",
		"Occluder full",
		"Reflection",
		"Crowd"
	};
	return object_type_names[ot];
}

std::string get_default_object_type_name(ObjectType ot, ObjectTypeSet ots)
{
	switch (ots) {
	case OTS_SEPIA: return get_Sepia_object_type_name(ot);
	case OTS_MOT20: return get_MOT20_object_type_name(ot);
	}
	return "user defined";
}

cgv::render::render_types::rgb get_color_from_hue(float value)
{
	cgv::media::color<float, cgv::media::HLS> hls(value - (int)value, 0.5f, 1.0f);
	return cgv::render::render_types::rgb(hls);
}

/// helper method to produce a color for a predefined object type
cgv::render::render_types::rgb get_default_object_color(ObjectType ot, ObjectTypeSet ots)
{
	return get_color_from_hue((float)(int)ot / get_nr_object_types(ots));
}

const std::vector<std::pair<std::string, cgv::render::render_types::rgb> >& get_default_object_type_defs(ObjectTypeSet ots)
{
	static std::vector<std::vector<std::pair<std::string, cgv::render::render_types::rgb> > > obj_type_set_defs;
	static bool initialized = false;
	if (!initialized) {
		obj_type_set_defs.resize(OTS_END);
		for (ObjectTypeSet ots = OTS_BEGIN; ots < OTS_END; ++(int&)ots) {
			for (ObjectType ot = OT_BEGIN; ot < get_nr_object_types(ots); ++(int&)ot)
				obj_type_set_defs[ots].push_back({ get_default_object_type_name(ot, ots), get_default_object_color(ot, ots) });
		}
		initialized = true;
	}
	return obj_type_set_defs[ots];
}

std::vector<std::pair<std::string, cgv::render::render_types::rgb> >& ref_object_type_defs()
{
	static std::vector<std::pair<std::string, cgv::render::render_types::rgb> > obj_type_defs;
	static bool initialized = false;
	if (!initialized) {
		obj_type_defs = get_default_object_type_defs(OTS_SEPIA);
		initialized = true;
	}
	return obj_type_defs;
}

size_t get_object_type_end()
{
	return ref_object_type_defs().size();
}
#include <cgv/utils/advanced_scan.h>
#include <cgv/utils/convert_string.h>

void video_labeling::update_object_types(const std::vector<std::pair<std::string, cgv::render::render_types::rgb> >& old_type_defs, const std::vector<std::pair<std::string, cgv::render::render_types::rgb> >& new_type_defs)
{
	for (auto& obj : objects) {
		if (obj.type != OT_USER_DEFINE)
			obj.type_name = old_type_defs[obj.type].first;
		obj.type = OT_USER_DEFINE;
		for (const auto& ot : new_type_defs) {
			if (ot.first == obj.type_name) {
				obj.type = ObjectType(&ot - &new_type_defs.front());
				//obj.type_name = "";
				break;
			}
		}
	}
}

void video_labeling::set_default_object_type_definitions(ObjectTypeSet ots)
{
	update_object_types(ref_object_type_defs(), get_default_object_type_defs(ots));
	ref_object_type_defs() = get_default_object_type_defs(ots);
}

bool video_labeling::read_object_type_definition(const std::string& file_path)
{
	std::string content;
	if (!cgv::utils::file::read(file_path, content, true))
		return false;
	std::vector<std::pair<std::string, cgv::render::render_types::rgb> > object_type_defs;
	object_type_defs.push_back({ "user defined", cgv::render::render_types::rgb(1,0,0) });
	float hue = 0.07f;
	float hue_delta = 0.07f;
	std::vector<cgv::utils::line> lines;
	cgv::utils::split_to_lines(content, lines);
	for (auto& l : lines) {
		std::vector<cgv::utils::token> tokens;
		cgv::utils::split_to_tokens(l, tokens, ",\t", false, "", "", "");
		if (tokens.size() == 1) {
			object_type_defs.push_back({ to_string(tokens[0]), get_color_from_hue(hue) });
			hue += hue_delta;
		}
		else if (tokens.size() == 3) {
			double h;
			if ((tokens[1] == "," || tokens[1] == "\t") && cgv::utils::is_double(to_string(tokens[2]), h)) {
				object_type_defs.push_back({ to_string(tokens[0]), get_color_from_hue(float(h)) });
				hue += float(h + hue_delta);
			}
			else {
				std::cerr << "line " << (&l - &lines.front()) + 1 << ": unknown <" << to_string(l) << ">" << std::endl;
			}
		}
		else if (tokens.size() == 7) {
			double r,g,b;
			if (
				(tokens[1] == "," || tokens[1] == "\t") &&
				cgv::utils::is_double(to_string(tokens[2]), r) &&
				(tokens[3] == "," || tokens[3] == "\t") &&
				cgv::utils::is_double(to_string(tokens[4]), g) &&
				(tokens[5] == "," || tokens[5] == "\t") &&
				cgv::utils::is_double(to_string(tokens[6]), b)
				) {
				object_type_defs.push_back({ to_string(tokens[0]), cgv::render::render_types::rgb(float(r),float(g),float(b)) });
			}
			else {
				std::cerr << "line " << (&l - &lines.front()) + 1 << ": unknown <" << to_string(l) << ">" << std::endl;
			}
		}
		else {
			std::cerr << "line " << (&l - &lines.front()) + 1 << ": unknown <" << to_string(l) << ">" << std::endl;
		}
	}
	// update current type names
	update_object_types(ref_object_type_defs(), object_type_defs);
	ref_object_type_defs() = object_type_defs;
	return true;
}

/// helper function to transform an object type into a string
std::string get_object_type_name(ObjectType ot)
{
	return ref_object_type_defs()[ot].first;
}

/// helper method to produce a color for a predefined object type
cgv::render::render_types::rgb get_object_color(ObjectType ot)
{
	return ref_object_type_defs()[ot].second;
}

/// helper function to construct an enum declaration used to define dropdown menus for object types
std::string get_object_type_name_enum_decl()
{
	std::string otn_ed("enums='");
	for (size_t ot = 0; ot < ref_object_type_defs().size(); ++ot) {
		if (ot != 0)
			otn_ed += ";";
		otn_ed += get_object_type_name(ObjectType(ot));
	}
	otn_ed += "'";
	return otn_ed;
}

tracked_object::tracked_object(ObjectType ot, const std::string& tn) : type(ot), type_name(tn) {}


std::string tracked_object::get_type_name() const
{
	if (type == OT_USER_DEFINE)
		return type_name;
	return get_object_type_name(type);
}

void control_value_vector_to_json(json& a, const std::string& name, const std::vector<control_value>& values)
{
	json& C = a[name] = json::array();
	for (const auto& cv : values) {
		C.push_back(json::object());
		json& c = C.back();
		c["value"] = cv.value;
		c["frame_idx"] = cv.frame_idx;
	}
}

void json_to_control_value_vector(const json& a, const std::string& name, std::vector<control_value>& values)
{
	const json& C = a[name];
	for (const auto& c : C)
		values.push_back(control_value(c["value"].get<float>(), c["frame_idx"].get<int>()));
}

bool video_labeling::save_object_file(const std::string& file_name) const
{
	json j;
	j["file_type"] = "tracked_objects";
	j["version"] = "1.0";
	j["width"] = width;
	j["height"] = height;
	j["nr_frames"] = nr_frames;
	j["objects"] = json::array();
	json& O = j["objects"];
	for (const auto& obj : objects) {
		O.push_back(json::object());
		json& o = O.back();
		o["type"] = obj.get_type_name();
		o["comment"] = obj.comment;
		json& A = (o["appearences"] = json::array());
		for (const auto& app : obj.appearences) {
			A.push_back(json::object());
			auto& a = A.back();
			a["fst_frame_idx"] = app.fst_frame_idx;
			a["lst_frame_idx"] = app.lst_frame_idx;
			control_value_vector_to_json(a, "x_min_values", app.x_min_values);
			control_value_vector_to_json(a, "x_max_values", app.x_max_values);
			control_value_vector_to_json(a, "y_min_values", app.y_min_values);
			control_value_vector_to_json(a, "y_max_values", app.y_max_values);
		}
	}

	std::string ext = cgv::utils::to_lower(cgv::utils::file::get_extension(file_name));

	if (ext == "json") {
		std::ofstream os(file_name);
		if (os.fail())
			return false;
		os << j;
	}
	else {
		std::vector<std::uint8_t> v;
		if (ext == "bson")
			v = json::to_bson(j);
		else if (ext == "cbor")
			v = json::to_cbor(j);
		else if (ext == "ubj")
			v = json::to_ubjson(j);
		else if (ext == "bjd")
			v = json::to_msgpack(j);
		else
			return false;
		return cgv::utils::file::write(file_name, (const char*)&v.front(), v.size());
	}
	return true;
}

bool video_labeling::save_MOT_file(const std::string& file_name) const
{
	std::ofstream os(file_name);
	if (os.fail())
		return false;
	for (unsigned frame_idx = 1; frame_idx <= nr_frames; frame_idx++) {
		int o_id = 1;
		for (const auto& obj : objects) {
			for (auto& app : obj.appearences) {
				if ((int)frame_idx >= app.fst_frame_idx && (int)frame_idx <= app.lst_frame_idx) {
					std::vector<control_value>::iterator iter;
					float xmin, xmax, ymin, ymax;
					bool flag = true;
					flag = extract_value(app.x_min_values, frame_idx, xmin);
					if (!flag)
						continue;
					flag = extract_value(app.x_max_values, frame_idx, xmax);
					if (!flag)
						continue;
					flag = extract_value(app.y_min_values, frame_idx, ymin);
					if (!flag)
						continue;
					flag = extract_value(app.y_max_values, frame_idx, ymax);
					if (!flag)
						continue;
					
					os << frame_idx << "," << o_id << "," << xmin << "," << ymin << "," << (xmax-xmin) << "," << (ymax-ymin) << "," << 1 << "," << -1 << "," << -1 << "," << -1 << std::endl;

					break;
				}
			}
			o_id++;
		}
	}
	return true;
}

bool video_labeling::open_object_file(const std::string& file_name)
{
	if (!cgv::utils::file::exists(file_name))
		return false;
	std::string ext = cgv::utils::to_lower(cgv::utils::file::get_extension(file_name));
	json j;
	if (ext == "json") {
		std::ifstream is(file_name);
		if (is.fail())
			return false;
		is >> j;
	}
	else {
		if (!cgv::utils::file::exists(file_name))
			return false;
		size_t size = cgv::utils::file::size(file_name);
		std::vector<std::uint8_t> v(size, 0);
		cgv::utils::file::read(file_name, (char*)&v.front(), size);
		if (ext == "bson")
			j = json::from_bson(v);
		else if (ext == "cbor")
			j = json::from_cbor(v);
		else if (ext == "ubj")
			j = json::from_ubjson(v);
		else if (ext == "bjd")
			j = json::from_msgpack(v);
		else
			return false;
	}
	if (j["file_type"].get<std::string>() != "tracked_objects") {
		cgv::gui::message("unknown json file type");
		return false;
	}
	if (j["version"].get<std::string>() != "1.0") {
		cgv::gui::message("unknown version, expected 1.0");
		return false;
	}
	objects.clear();
	for (const auto& o : j["objects"]) {
		std::string tn = o["type"].get<std::string>();
		ObjectType ot = OT_USER_DEFINE;
		for (size_t t = 0; t < ref_object_type_defs().size(); ++t) {
			if (tn == get_object_type_name(ObjectType(t))) {
				ot = ObjectType(t);
				break;
			}
		}
		//if (ot != OT_USER_DEFINE)
		//	tn = "";
		objects.push_back(tracked_object(ot, tn));

		tracked_object& obj = objects.back();
		obj.comment = o["comment"].get<std::string>();
		const json& A = o["appearences"];
		for (const auto& a : A) {
			obj.appearences.push_back(appearance());
			appearance& app = obj.appearences.back();
			app.fst_frame_idx = a["fst_frame_idx"].get<int>();
			app.lst_frame_idx = a["lst_frame_idx"].get<int>();
			json_to_control_value_vector(a, "x_min_values", app.x_min_values);
			json_to_control_value_vector(a, "x_max_values", app.x_max_values);
			json_to_control_value_vector(a, "y_min_values", app.y_min_values);
			json_to_control_value_vector(a, "y_max_values", app.y_max_values);
		}
	}
	return true;
}

bool video_labeling::extract_value(const std::vector<control_value>& controls, int frame_idx, float& value) const
{
	for (size_t i = 0; i < controls.size(); ++i) {
		const auto& cv = controls[i];
		if (cv.frame_idx == frame_idx) {
			value = cv.value;
			return true;
		}
		if (cv.frame_idx > frame_idx) {
			if (i == 0)
				return false;
			const auto& cp = controls[i - 1];
			float lambda = float(frame_idx - cp.frame_idx) / (cv.frame_idx - cp.frame_idx);
			value = (1.0f - lambda) * cp.value + lambda * cv.value;
			return true;
		}
	}
	return false;
}

/// find and modify or insert
bool video_labeling::ensure_control_value(std::vector<control_value>& video_storage, int frame_idx, float value, float** value_ptr_ptr)
{
	std::vector<control_value>::iterator iter;
	if (!find_control_value(video_storage, frame_idx, iter)) {
		iter = video_storage.insert(iter, control_value(value, frame_idx));
		if (value_ptr_ptr)
			*value_ptr_ptr = &iter->value;
		return true;
	}
	else {
		iter->value = value;
		if (value_ptr_ptr)
			*value_ptr_ptr = &iter->value;
		return false;
	}
}

bool video_labeling::find_control_value(std::vector<control_value>& video_storage, int fi, std::vector<control_value>::iterator& iter)
{
	for (size_t i = 0; i < video_storage.size(); ++i)
		if (fi <= video_storage[i].frame_idx) {
			iter = video_storage.begin() + i;
			return fi == video_storage[i].frame_idx;
		}
	iter = video_storage.end();
	return false;
}

/// remove all values before given start frame index
void video_labeling::offset_values(std::vector<control_value>& v, int sfi) const
{
	for (size_t i = 0; i < v.size(); ++i) {
		const auto& cv = v[i];
		if (cv.frame_idx < sfi)
			continue;
		if (cv.frame_idx > sfi) {
			if (i > 0) {
				auto& cp = v[--i];
				float lambda = float(sfi - cp.frame_idx) / (cv.frame_idx - cp.frame_idx);
				cp.value = (1.0f - lambda) * cp.value + lambda * cv.value;
				cp.frame_idx = sfi;
			}
		}
		if (i > 0)
			v.erase(v.begin(), v.begin() + i);
		break;
	}
}

/// truncate all values after given end frame index
void video_labeling::truncate_values(std::vector<control_value>& v, int efi) const
{
	for (size_t i = 0; i < v.size(); ++i) {
		auto& cv = v[i];
		if (cv.frame_idx < efi)
			continue;
		if (cv.frame_idx > efi) {
			if (i > 0) {
				const auto& cp = v[i - 1];
				float lambda = float(efi - cp.frame_idx) / (cv.frame_idx - cp.frame_idx);
				cv.value = (1.0f - lambda) * cp.value + lambda * cv.value;
				cv.frame_idx = efi;
			}
		}
		if (i + 1 < v.size())
			v.erase(v.begin() + i + 1, v.end());
		break;
	}
}


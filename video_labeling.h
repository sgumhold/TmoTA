#pragma once

#include <vector>
#include <string>
#include <cgv/media/axis_aligned_box.h>
#include <cgv/render/render_types.h>

/// index of first frame (0 would be C/C++ convention, 1 java)
const int fst_frame_idx = 1;
/// index of first pixel
const int fst_pixel_idx = 1;
/// index of first pixel
const int fst_object_idx = 1;
/// index of first pixel
const int fst_type_idx = 1;
/// whether to store subpixel precise pixel values
const bool allow_subpixel_precision = false;
/// type of a rectangle with integer coordinates
typedef cgv::media::axis_aligned_box<int, 2> ibox2;

/// for one of the four rectangle bounds a value with a frame index used as control point for interpolation
struct control_value
{
	float value;
	int frame_idx;
	/// construction from value and index
	control_value(float v, int fi) : value(v), frame_idx(fi) {}
};

/// an appearance stores an interval of frames where an object appeared in the video
struct appearance
{
	/// first frame of appearance
	int fst_frame_idx;
	/// last frame of appearance
	int lst_frame_idx;
	/// control points for left rectangle bound
	std::vector<control_value> x_min_values;
	/// control points for right rectangle bound
	std::vector<control_value> x_max_values;
	/// control points for top rectangle bound
	std::vector<control_value> y_min_values;
	/// control points for bottom rectangle bound
	std::vector<control_value> y_max_values;
	/// return last rectangle (x_min,y_min,x_max,y_max) in appearance
	cgv::math::fvec<float, 4> last_rectangle() const {
		return cgv::math::fvec<float, 4>(x_min_values.back().value, y_min_values.back().value, x_max_values.back().value, y_max_values.back().value);
	}
	/// append rectangle (x_min,y_min,x_max,y_max) in appearance
	void append_rectangle(const cgv::math::fvec<float, 4>& r, int fi) {
		x_min_values.push_back(control_value(r(0), fi));
		y_min_values.push_back(control_value(r(1), fi));
		x_max_values.push_back(control_value(r(2), fi));
		y_max_values.push_back(control_value(r(3), fi));
	}
	/// set last rectangle (x_min,y_min,x_max,y_max) in appearance
	void set_last_rectangle(const cgv::math::fvec<float, 4>& r, int fi) {
		x_min_values.back() = control_value(r(0), fi);
		y_min_values.back() = control_value(r(1), fi);
		x_max_values.back() = control_value(r(2), fi);
		y_max_values.back() = control_value(r(3), fi);
	}
	/// return first rectangle (x_min,y_min,x_max,y_max) in appearance
	cgv::math::fvec<float, 4> first_rectangle() const {
		return cgv::math::fvec<float, 4>(x_min_values.front().value, y_min_values.front().value, x_max_values.front().value, y_max_values.front().value);
	}
	/// prepend rectangle (x_min,y_min,x_max,y_max) in appearance
	void prepend_rectangle(const cgv::math::fvec<float, 4>& r, int fi) {
		x_min_values.insert(x_min_values.begin(), control_value(r(0), fi));
		y_min_values.insert(y_min_values.begin(), control_value(r(1), fi));
		x_max_values.insert(x_max_values.begin(), control_value(r(2), fi));
		y_max_values.insert(y_max_values.begin(), control_value(r(3), fi));
	}
	/// set first rectangle (x_min,y_min,x_max,y_max) in appearance
	void set_first_rectangle(const cgv::math::fvec<float, 4>& r, int fi) {
		x_min_values.front() = control_value(r(0), fi);
		y_min_values.front() = control_value(r(1), fi);
		x_max_values.front() = control_value(r(2), fi);
		y_max_values.front() = control_value(r(3), fi);
	}
};

/// enumerate for predefined object types
enum ObjectType
{
	OT_BEGIN,
	OT_USER_DEFINE = OT_BEGIN,
	OT_CAR,
	OT_TRUCK,
	OT_BUS,
	OT_BYCICLE,
	OT_MOTORCYCLE,
	OT_TRAILER,
	OT_TRAM,
	OT_TRAIN,
	OT_CARAVAN,
	OT_AGRICULTURAL_VEHICLE,
	OT_CONSTRUCTION_VEHICLE,
	OT_EMERGENCY_VEHICLE,
	OT_PASSIVE_VEHICLE,
	OT_PERSON,
	OT_LARGE_ANIMAL,
	OT_SMALL_ANIMAL,
	OT_END
};

/// helper function to transform an object type into a string
extern std::string get_object_type_name(ObjectType ot);

//extern size_t get_object_type_begin();
extern size_t get_object_type_end();

/// helper method to produce a color for a predefined object type
extern cgv::render::render_types::rgb get_object_color(ObjectType ot);

/// helper function to construct an enum declaration used to define dropdown menus for object types
extern std::string get_object_type_name_enum_decl();

/// class to store all information for a single object seen in the video
struct tracked_object
{
	/// predefined object type 
	ObjectType type;
	/// in case of no predefined type, the type name of the new object class
	std::string type_name;
	/// an optional comment
	std::string comment;
	/// a vector of appearances for this object
	std::vector<appearance> appearences;
	/// constructor for an object based on its type
	tracked_object(ObjectType ot, const std::string& tn = "");
	/// return the type name 
	std::string get_type_name() const;
};

enum ObjectTypeSet
{
	OTS_SEPIA,
	OTS_MOT20,
	OTS_END,
	OTS_BEGIN = OTS_SEPIA
};

/// information stored in json file for the labeling of a single video
class video_labeling
{
protected:
	/// width in pixel of video frames
	unsigned width;
	/// height in pixel of video frames
	unsigned height;
	/// total number of video frames
	unsigned nr_frames;
	/// list of all labeled objects
	std::vector<tracked_object> objects;
	/// update all object types to a new list of object types
	void update_object_types(const std::vector<std::pair<std::string, cgv::render::render_types::rgb> >& old_type_defs, const std::vector<std::pair<std::string, cgv::render::render_types::rgb> >& new_type_defs);
public:
	/// read object type definition file and update object types
	bool read_object_type_definition(const std::string& file_path);
	/// revert to the default object type definitions
	void set_default_object_type_definitions(ObjectTypeSet ots = OTS_SEPIA);
	/// save the video labeling data structure to a json file, detect format from extension (json, bson, cbor, ubj, bjd)
	bool save_object_file(const std::string& file_name) const;
	/// save the video labeling data structure to a MOT file
	bool save_MOT_file(const std::string& file_name) const;
	/// read video labeling data structure from ascii or binary json file, detect format from extension (json, bson, cbor, ubj, bjd)
	bool open_object_file(const std::string& file_name);
	/// compute the index of the last frame
	int lst_frame_idx() const { return (int)nr_frames + fst_frame_idx - 1; }
	/// extract an potentially interpolated value at a given frame index of a control value vector
	bool extract_value(const std::vector<control_value>& controls, int frame_idx, float& value) const;
	/// if control vector contains value for given frame index, set iter to its location and return true; otherwise set iter to location where value for given frame index should be inserted and return false
	bool find_control_value(std::vector<control_value>& video_storage, int frame_idx, std::vector<control_value>::iterator& iter);
	/// find and modify or insert
	bool ensure_control_value(std::vector<control_value>& video_storage, int frame_idx, float value, float** value_ptr_ptr = 0);
	/// remove all values before given start frame index
	void offset_values(std::vector<control_value>& controls, int start_frame_idx) const;
	/// remove all values after given end frame index
	void truncate_values(std::vector<control_value>& controls, int end_frame_idx) const;

};

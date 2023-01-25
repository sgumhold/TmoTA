#include <cgv/base/node.h>
#include <cgv/base/node.h>
#include <cgv/gui/provider.h>
#include <cgv/gui/event_handler.h>
#include <cgv/gui/key_event.h>
#include <cgv/gui/trigger.h>
#include "TmoTA_drawable.h"
#include <cgv/os/mutex.h>
#include <cgv/os/thread.h>

struct mouse_over_mode
{
	int mode = 0;
	int mouse_x = 0, mouse_y = 0;
	int x_mode = 0, y_mode = 0;
	int rect_offset_x = 0, rect_offset_y = 0;
	int rect_width = 1, rect_height = 1;
	int new_object_idx = -1;
	int nr_objects = 0;
	bool in_video_viewport = false;
	bool connect_left = false;
	bool connect_right = false;
};

/// the interactable implements animations, handles key and mouse events, and creates a ui
class TmoTA_interactable :
	public cgv::base::node, // incorporates interactable to object hierarchy
	public TmoTA_drawable,
	public cgv::gui::provider,
	public cgv::gui::event_handler
{
	/**@name extension of label_tool_drawable for interaction purposes*/
	//@{
protected:
	/// store selected font
	cgv::media::font::font_face_ptr ff;
	/// index into font name list
	unsigned int font_idx;
	/// store font size
	float font_size;
	///
	rgb font_color;
	/// whether font should be bold
	bool bold;
	/// whether font should be italic
	bool italic;
	/// store list of font names
	std::vector<const char*> font_names;

	/// ensure ui up to date after opening a directory
	bool open_directory(const std::string& path, const std::string& ext = "jpg");

	void set_object_file_name(const std::string& file_name);
	bool use_asynchronous_read = true;
	bool has_new_volume = false;
	unsigned last_file_idx = 0;

	static void on_progress_update_callback(int, void*);
	void on_progress_update(int);

	friend struct read_thread;
	
	struct read_thread : public cgv::os::thread
	{
		std::string file_name;
		std::string component_format;

		static void on_progress_update_callback(int, void*);
		void on_progress_update(int);

		int progress = -1;
	
		mutable cgv::os::mutex mtx;
		TmoTA_interactable* lti;
		read_thread(TmoTA_interactable* _lti, const std::string& fn, const std::string& cf) : lti(_lti), file_name(fn), component_format(cf) {}
		int query_progress() const;
		void run();
	};
	read_thread* read_trd = 0;
	/// start thread that ready video file asynchronously
	bool start_asynchronous_read(const std::string& file_name);
	/// allocate video storage for given format / dimensions and return number of frames that fit into system memory
	size_t configure_video_storage(const cgv::data::component_format& cf, size_t w, size_t h, size_t n);
	/// helper function to center view
	void ensure_centered_view();
	/// helper function to remove all frames from appearance before given start frame index
	void offset_appearance(appearance& app, int start_frame_idx);
	/// helper function to remove all frames from appearance behind given end frame index
	void truncate_appearance(appearance& app, int end_frame_idx);
public:
	/// we start our trigger here
	bool init(cgv::render::context& ctx);
	/// ensure that file index is updated in ui
	void init_frame(cgv::render::context& ctx);
	/// specialize draw method to current object and appearance indices
	void draw(cgv::render::context& ctx);
	void finish_frame(cgv::render::context& ctx);
	//@}

	/**@name animation*/
	//@{
protected:
	/// trigger object that allows to call timer_event with 200 Hz
	cgv::gui::trigger trig;
	/// timer event used to animate the label tool
	void timer_event(double t, double dt);
	/// index of current frame
	unsigned frame_idx;
	/// color of currently triggered color impuls 
	rgb impuls_color;
	/// blend factor for current color impuls
	float impuls_lambda;
	/// start a color impuls with the clear color
	void color_impuls(const rgb& col);
	/// define clear color from impuls color and lambda
	rgb get_clear_color() const;
private:
	/// time of last timer event used to determine whether animation should go to frame 
	double last_t;
	/// whether animation is stepping forward
	bool forward;
protected:
	std::string object_type_file_name;
	void set_def_types(ObjectTypeSet ots);
	/// frames per second with which to drive through video 
	float fps;
	/// whether animation is active
	bool animate;
	/// whether animation is restricted to current appearance
	bool restrict_animation;
	/// whether to center the view on the rectangle of the current object
	bool auto_center_view;
	//@}

public:
	/**@name cgv::base::node implementation*/
	//@{
	/// constructor initializes members
	TmoTA_interactable();
	/// callback called by ui and config file 
	void on_set(void* member_ptr);
	/// return label_tool as the type name
	std::string get_type_name() const;
	/// reflect members that should be accessible through config file
	bool self_reflect(cgv::reflect::reflection_handler& rh);
	//@}

	/**@name event handling*/
	//@{
private:
	/// remember whether last action was a left mouse button press, which is falsified as soon as a drag event happens
	bool potential_left_click;
	/// remember whether last action was a right mouse button press, which is falsified as soon as a drag event happens
	bool potential_right_click;
	/// whether to extent appearances
	bool extent_appearance = true;
	int open_appearance_object_idx = -1;
	int open_appearance_idx = -1;
	///
	bool left_clicked = false;
	bool right_clicked = false;
	/// whether left ctrl is pressed
	bool left_ctrl = false;
	/// whether right ctrl is pressed
	bool right_ctrl = false;
	/// last mouse pixel location
	int last_x, last_y;
protected:
	/// modifier for mouse interaction telling whether start click of rectangle definition is center or otherwise top left corner
	bool span_rectangle_from_center;

	char rectangle_key;
	vec2 rectangle_pix;

	int x_mode, y_mode;
	//int rect_width = 1, rect_height = 1;
	int new_object_idx = -1;

	mouse_over_mode over_mode;

	mouse_over_mode determine_over_mode(int x, int y) const;
	void update_over_mode(int x, int y, bool in_video_viewport);

	bool pressed_in_video_viewport = false;
	bool pressed_in_object_viewport = false;
	int pick_width = 25;
	int obj_layer = 0;
protected:
	void do_ensure_control_value(std::vector<control_value>& video_storage, int frame_idx, float value);
	/// coordinate transformation functions
	float model_x_to_texcoord_u(float x) const { return 0.5f * (x / aspect + 1.0f); }
	float model_y_to_texcoord_v(float y) const { return 0.5f * (1.0f - y); }
	vec2 model_to_texcoord(const vec2& p) const { return vec2(model_x_to_texcoord_u(p(0)), model_y_to_texcoord_v(p(1))); }
	float texcoord_u_to_model_x(float u) const { return (2.0f * u - 1.0f) * aspect; }
	float texcoord_v_to_model_y(float v) const { return 1.0f - 2.0f * v; }
	vec2 texcoord_to_model(const vec2& tc) const { return vec2(texcoord_u_to_model_x(tc(0)), texcoord_v_to_model_y(tc(1))); }
	float texcoord_u_to_pixelcoord_x_f(const float u) const { return u * width + (float)fst_pixel_idx; }
	float texcoord_v_to_pixelcoord_y_f(const float v) const { return v * height + (float)fst_pixel_idx; }
	vec2 texcoord_to_pixelcoord_f(const vec2& tc) const { return vec2(texcoord_u_to_pixelcoord_x_f(tc(0)), texcoord_v_to_pixelcoord_y_f(tc(1))); }
	int texcoord_u_to_pixelcoord_x(const float u) const { return (int)floor(texcoord_u_to_pixelcoord_x_f(u)); }
	int texcoord_v_to_pixelcoord_y(const float v) const { return (int)floor(texcoord_v_to_pixelcoord_y_f(v)); }
	ivec2 texcoord_to_pixelcoord(const vec2& tc) const { return ivec2(texcoord_u_to_pixelcoord_x(tc(0)), texcoord_v_to_pixelcoord_y(tc(1))); }
	float pixel_x_to_texcoord_u(int x, int mode = 0) const { return (x - fst_pixel_idx + 0.5f * (1 + mode)) / width; }
	float pixel_y_to_texcoord_v(int y, int mode = 0) const { return (y - fst_pixel_idx + 0.5f * (1 + mode)) / height; }
	vec2 pixel_to_texcoord(const ivec2& pix, int mode_x = 0, int mode_y = 0) const { return vec2(pixel_x_to_texcoord_u(pix(0), mode_x), pixel_y_to_texcoord_v(pix(1), mode_y)); }
	float pixel_x_to_texcoord_u_f(float x, int mode = 0) const { return (x - (float)fst_pixel_idx + 0.5f * (1 + mode)) / width; }
	float pixel_y_to_texcoord_v_f(float y, int mode = 0) const { return (y - (float)fst_pixel_idx + 0.5f * (1 + mode)) / height; }
	vec2 pixel_to_texcoord_f(const vec2& pix, int mode_x = 0, int mode_y = 0) const { return vec2(pixel_x_to_texcoord_u_f(pix(0), mode_x), pixel_y_to_texcoord_v_f(pix(1), mode_y)); }

	/// find picked world location from mouse pixel location or return false if mouse points outside of video frame
	bool find_pick_location(int mouse_x, int mouse_y, vec2& p) const;
	/// find picked 2D texture coordinate from mouse pixel location or return false if mouse points outside of video frame
	bool find_picked_texcoord(int mouse_x, int mouse_y, vec2& tc) const;
	/// find picked video pixel location from mouse pixel location or return false if mouse points outside of video frame
	bool find_picked_pixel(int mouse_x, int mouse_y, vec2& pix) const;
	/// determine mouse interaction modes
	int determine_mode(int x, int y, int object_idx, int frame_idx, int& x_mode, int& y_mode, int* x_off_ptr = 0, int* y_off_ptr = 0, int* width_ptr = 0, int* height_ptr = 0) const;
	/// find objects under mouse pointer
	std::vector<int> find_objects(int mx, int my) const;

	/// function to select a specific frame of the video
	bool select_frame(int x);

	/// append a one-pixel rectangle at the end of an appearance assuming that given frame index is larger than last appearance's frame index
	void append_rectangle(appearance& app, int x, int y, int frame_idx);
	/// add a new appearance starting at given frame index, if select parameter is true, select appearance as current
	int add_appearance(tracked_object& obj, int frame_idx, bool select = false);
	/// add appearance with a one-pixel rectangle, if select parameter is true, select appearance as current
	int add_appearance(tracked_object& obj, int frame_idx, int x, int y, bool select = false);
	/// add a new object, if select parameter is true, select object as current
	int add_object(ObjectType ot, const std::string& type_name, bool select = false);
	/// add a new object with a one-pixel rectangle, if select parameter is true, select object as current
	int add_object(ObjectType ot, const std::string& type_name, int x, int y, bool select = false);
	/// update the current object's rectangle based on the interaction state and the current mouse location
	bool update_location(int mouse_x, int mouse_y);
	/// support function to handle a modifier key
	bool handle_modifier_key(int mx, int my, cgv::gui::KeyAction ka);
	/// support function to handle rectangle modifier key
	bool handle_rectangle_modifier_key(cgv::gui::KeyAction ka, char key);
public:
	/// handle key and mouse events
	bool handle(cgv::gui::event& e);
	/// stream out help on the interaction posibilities
	void stream_help(std::ostream& os);
	/// stream out statistical information
	void stream_stats(std::ostream& os);
	//@}

	/**@name ui creation*/
protected:
	//@{
	/// index of currently edited object 
	int object_idx;
	/// index of currently edited object appearance
	int appearance_idx;
	/// index of currently edited control value for x_min control vector
	int x_min_idx;
	/// index of currently edited control value for x_max control vector
	int x_max_idx;
	/// index of currently edited control value for y_min control vector
	int y_min_idx;
	/// index of currently edited control value for y_max control vector
	int y_max_idx;
	/// set slider ranges of controls according to currently set indices
	void configure_controls();
	void create_object_gui();
	void create_object_gui_2();

public:
	/// creation of ui elements
	void create_gui();
	//@}
};

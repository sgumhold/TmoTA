#include "video_labeling.h"
#include <cgv/render/drawable.h>
#include <cgv/render/attribute_array_binding.h>
#include <cgv/render/texture.h>
#include <cgv/media/volume/volume.h>
#include <cgv/render/shader_program.h>
#include <cgv_gl/gl/gl.h>

enum class json_extension
{
	json, bson, cbor, ubj, bjd
};

std::string get_json_extension(json_extension ext);

/// the label tool drawable is responsible for reading the video and drawing the video and the time line in separate viewports
class TmoTA_drawable :
	public video_labeling, 
	public cgv::render::drawable
{
protected:
	int x_over_mode = 0, y_over_mode = 0;
	int over_object_index = -1;
	/**@name video storage */
	//@{
	cgv::data::component_format video_cf;
	size_t frame_size = 0;
	size_t nr_read_frames = 0;
	size_t nr_video_tex_frames = 0;
	size_t max_video_storage_size = 4000000000;
	/// format of frame
	cgv::data::data_format frame_format;
	/// format of the whole video
	cgv::data::data_format video_format;
	/// data container for to be transmitted video data (which is typically again just for a single video frame)
	cgv::data::data_view video_data;
	/// number of frames read into video_data member
	size_t nr_available_frames = 0;
	/// store video in volume data structure
	cgv::media::volume::volume video_storage;
	/// flag whether to use frame texture or video texture
	bool use_video_tex = false;
	/// 3d texture to store the video
	cgv::render::texture video_tex;
	/// 2d texture to store a single video frame
	cgv::render::texture frame_tex;
	/// allocate video storage for given format / dimensions and return number of frames that fit into system memory
	virtual size_t configure_video_storage(const cgv::data::component_format& cf, size_t w, size_t h, size_t n);
	//@}

	/**@name reading video data*/
	//@{
	/// path the video image files
	std::string path;
	/// image format of a single video frame
	cgv::data::data_format df;
	/// data container for a single video frame
	cgv::data::data_view dv;
	/// read from video file
	std::string video_file_name;
	/// all image file names 
	std::vector<std::string> file_names;
	/// index of next to be read image file
	size_t file_idx;
	/// size of an image after decompression
	size_t image_size;
	/// file name of the json file to store the video labeling
	std::string json_file_name;
	/// file name extension used for json files
	json_extension object_file_extension = json_extension::json;
	///
	std::string get_json_file_name() const { return json_file_name + "." + get_json_extension(object_file_extension); }
	/// extract all file names for a given file filter
	void glob(const std::string& filter, std::vector<std::string>& file_names);
	/// open the image files with given extension from the directory given by the path parameter
	virtual bool open_directory(const std::string& path, const std::string& ext = "jpg");
	//@}

	/**@name video texture and shader programs */
	//@{
	/// mixing factor used to mix object color with video image color
	float mix_factor;
	/// width of rectangle border
	float rectangle_border_width;
	///
	bool focus_current = false;
	/// gamma for color intensity drop off at rectangle border
	float border_gamma;
	/// shader program to render the video frame with the current object in focus
	cgv::render::shader_program tex_prog;
	/// shader program to render all other rectangles around objects that have been labeled in the current frame
	cgv::render::shader_program rect_prog;
	//@}

	/**@name viewport management*/
	//@{
	/// whether to split main viewport vertically
	bool vertical_viewport_split = false;
	/// object width scale
	float object_width_scale = 0.95f;
	/// width of border around viewports measured in pixels
	int border_width = 5;
	/// height of object time viewport in pixels
	int object_viewport_extent = 200;
	/// viewport definition of video image viewport
	ivec4 video_viewport;
	/// aspect ratio of video image viewport
	float video_aspect;
	/// viewport definition of object time line viewport
	ivec4 object_viewport;
	/// aspect ratio of object time line viewport
	float object_aspect;
	//@}

	/**@name viewing and picking*/
	//@{
	/// focus point in world coordinates that is shown in center of view in top viewport
	vec2 view_focus;
	/// view scaling of view in top viewport
	float view_scale;
	/// aspect ratio of view in top viewport
	float aspect;
	/// matrix used to compute world coordinates from mouse pixel coordinates
	dmat4 MPW;
	//@}

	/// color used to clear opengl window which is kept on border around viewports
	virtual rgb get_clear_color() const;
	/// draw geometry consisting of locations P and colors C with given shader program and with given opengl primitive type
	void draw_colored_geometry(cgv::render::context& ctx, cgv::render::shader_program& prog, GLenum prim_type, const std::vector<vec3>& P, const std::vector<rgb>& C);
	/// draw the current video frame with the current object inversely marked
	void draw_texture(cgv::render::context& ctx, int object_idx, int appearance_idx, unsigned frame_idx);
	/// draw rectangles for all objects present in the current frame
	void draw_rectangles(cgv::render::context& ctx, int object_idx, unsigned frame_idx);
	/// helper function to add the corner positions and colors of a rectangle to the given vectors P and C
	void add_rectangle(float x0, float y0, float x1, float y1, const rgb& col, std::vector<vec3>& P, std::vector<rgb>& C);
	/// convert a frame index to an x coordinate used for rendering on the time line
	float frame_idx_to_x(int frame_idx) const;
	/// draw the time line with the objects
	void draw_object_time_line(cgv::render::context& ctx, int object_idx, int appearance_idx, unsigned frame_idx);
public:
	/// initialize all members
	TmoTA_drawable();
	/// called once to init render objects
	bool init(cgv::render::context& ctx);
	/// destruct all render objects
	void clear(cgv::render::context& ctx);
	/// clear frame buffer and recreate 3D texture or if necessary upload image files to 3D texture
	void init_frame(cgv::render::context& ctx);
	/// draw all views
	void draw(cgv::render::context& ctx, int object_idx, int appearance_idx, unsigned frame_idx);
};

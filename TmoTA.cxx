#include "TmoTA_interactable.h"
#include <cgv/utils/dir.h>
#include <cgv/utils/file.h>
#include <cgv/base/register.h>
#include <cgv/gui/application.h>

/// the label tool extents the label_tool_interactable by command line argument processing
class TmoTA :
	public TmoTA_interactable,
	public cgv::base::argument_handler
{
public:
	void handle_args(std::vector<std::string>& args)
	{
		for (const auto& a : args) {
			if (cgv::utils::dir::exists(a) && open_directory(a)) {
				update_member(&nr_frames);
				update_member(&path);
				configure_controls();
				break;
			} else if (cgv::utils::file::exists(a)) {
				video_file_name = a;
				on_set(&video_file_name);
				break;
			}
		}
	}
	void on_register()
	{
		TmoTA_interactable::on_register();
		cgv::gui::application::get_window(0)->set("title", "Multiple Object Tracking Annotation Tool");
	}
public:
	TmoTA()
	{
	}
};

/// register instance of label too to framework
cgv::base::object_registration<TmoTA> TmoTA_reg("");

(C)2019-2023 Stefan Gumhold
# TmoTA
Tool for 2D multiple object Tracking Annotation introduced in the paper:

*Marzan Tasnim Oyshi, Sebastian Vogt, Stefan Gumhold: TmoTA: Simple, Highly Responsive Tool for Multiple Object Tracking Annotation; conditionally accepted to CHI 2023*

For use of the tool, please have a look at the license file LICENSE.

## dependencies
- ffmepg: https://ffmpeg.org
- cgv framework: https://github.com/sgumhold/cgv.git

## installation
- download archive with precompiled executable
  - Windows: https://cloudstore.zih.tu-dresden.de/index.php/s/fZmdCSMyekbSonq
  - Linux:
- optionally configure TmoTA executable by editing the file TmoTA.cfg (see comments inside)
- ensure that ffmpeg executable is found by operating system or download and copy executable from ffmpeg site to same directory as executable of TmoTA
- optionally download archive with sample videos and labelings used for the user studies in the CHI paper: https://cloudstore.zih.tu-dresden.de/index.php/s/XoPxe36MyzdJXfm
- drag and drop video onto TmoTA executable and press F2 to save changes


## Compile from Source
- clone repository: https://github.com/sgumhold/TmoTA
- use CMake on CMakeLists.txt in repository root directory (this will clone automatically the cgv framework into the cmake build directory)
- alternatively, you can use cgv framework's solution generation approach that supports different versions of Visual Studio with the project file TmoTA.pj

## Manual

optionally convert video to images via ffmpeg: `ffmpeg -i MOT16-05-raw.webm -f image2 -q:v 1 image%%04d.jpg`

### start options
- command line: TmoTA [video_file|frame_directory]
- drag and drop video file or frame directory onto TmoTA executable

### Viewing
In **Video View** use `Right Mouse Button` to ***pan*** and `Mouse Wheel` to ***zoom***.
 
### Time Navigation
- `Left`|`Right` step frame, use `Shift-` to step by 10 frames
- `Home`|`End` to jump to beginning / end of video or selected object appearance
- `Left Drag` over **Object View** to select frame
 
### Selection of Object / Appearance
- `Up`|`Down` to navigate through objects
- `Escape` to unselect currently selected object
- `Left Click` over **Object View** to select/unselect object/appearance
 
### Label Validation 
- `Space` to toggle ping pong playback
- `PgUp`|`PgDn` to adjust playback speed
- `D` to toggle playback direction
- `R` to restrict ping pong playback to selected object appearance
- `C` to toggle view centering

### Labeling
- `F2` to save current state in json file with name derived from video file or image directory
- `F` to toggle focusing on current object by hiding others
- change object type in object list of classical UI on the right side
- `Left Mouse Button`
 - inside rectangle of selected object: move rectangle
 - on rectangle boundary of selected object: update highlighted corner or edge
 - inside rectangle of not selected object: select new object
 - outside of rectangle check for mouse hint
   - `+O` ... add object and open appearance by dragging rectangle
   - `[A` ... begin new object appearance by dragging rectangle
   - `A]` ... end opended object appearance by dragging rectangle
   - `A]->` ... (only while `RightCtrl` pressed) extent preceeding appearance to current frame
   - `<-[A` ... (only while `LeftCtrl` pressed) extent succeeding appearance to current frame
- `Ctrl-Mouse Wheel` over overlapping rectangles cycles top layered rectangle
- `Delete` to make current appearance end at current frame by triming
- `BackSpace` to make selected appearance begin at current frame by triming
- `Alt-Delete` to remove selected appearance
- `Ctrl-Delete` to remove selected object

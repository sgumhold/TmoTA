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
  - Linux: soon
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

### Viewing and General Usage
In **Video View** use `Right Mouse Button` to ***pan*** and `Mouse Wheel` to ***zoom***.
For the classical UI elements tooltips are shown when resting mouse pointer of them. These also contain information on shortcuts.

- `F1` to show help
- `F2` to save current state in json file with name derived from video file or image directory
- `F3`to export in MOT format. File name extends json file name with "_MOT" and uses extension "txt".
- `Alt-F4` to terminate TmoTA
- `F8` to toggle statisitics
- `F11` to toggle fullscreen
 
### Time Navigation
- `Left`|`Right` step frame, use `Shift-` to step by 10 frames
- `Home`|`End` to jump to beginning / end of video or selected object appearance
- `Left Drag` over **Object View** to select frame
 
### Labeling

#### Creation of Appearances and Objects
Objects have a type (car, person, ...) and several appearances composed of frame interval and per frame bounding rectangle. To label an object, navigate videoframes in object view to find first frame of first appearance. Use left mouse button with press-drag-release interaction to draw rectangle around object. Next navigate frames to last frame of first appearance. The rectangle of the first frame is shown for reference with thin border. On last frame mark object again with press-drag-release interaction. This terminates the appearance and defines the rectangle for all frames between first and last by linear interpolation of the rectangle edge coordinates. If appearance is not terminated before selecting different object or starting a new object (by navigating before first frame of appearance and performing press-drag-release interaction), the un-terminated appearance is automatically deleted and if this was the first appearance of an object, the now empty object is also deleted.

#### Label Refinement
After definition of start and end rectangle of an appearance the interpolated rectangles need to be refined. Refinement is restricted to a single *selected object*. The new object is automatically selected during and after definition of an appearance. The selected object's bounding rectangle is shown inverted such that the object is seen in its original color and the rest of the video frame with the color corresponding to the object type, that can be edited through the classic UI on the right side.

To minimize labelling effort, a good strategy is to navigate frames to a spot with maximum discrepancy between the interpolated rectangle and the real bounding rectangle of the object in the video frame. To correct the rectangle, approach it with mouse pointer and pay attention to boundary edge highlighting. Highlighted edges can be edited by press-drag-release interaction or in case of one or two highlighted edges by clicking the target position. The UI parameter `pick_width` defines up to which pixel distance around rectangle edges click-interaction works.

#### Labeling Shortcuts
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
- `Ctrl-Mouse Wheel` over overlapping rectangles cycles top layered rectangle (see text hint showing current top rectangle index and total number of overlapping rectangles in the form `2(3)`).
- `Delete` to make current appearance end at current frame by triming
- `BackSpace` to make selected appearance begin at current frame by triming
- `Alt-Delete` to remove selected appearance
- `Ctrl-Delete` to remove selected object

### Selection of Object / Appearance
- `Up`|`Down` to navigate through objects
- `Escape` to unselect currently selected object
- `Left Click` over **Video View** or **Object View** to select/unselect object/appearance. In Video View the press-drag-release interaction can directly be applied to other objects also, what also selects this object (currently this does not always work when before frames had been navigated. This bug needs to be fixed)
 
### Label Validation 
- `Space` to toggle ping pong playback
- `PgUp`|`PgDn` to adjust playback speed
- `D` to toggle playback direction
- `R` to restrict ping pong playback to selected object appearance
- `C` to toggle view centering

## ToDo List
- support undo and redo
- support insertion of appearances
- fix other object press-drag-release interaction bug

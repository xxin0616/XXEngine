# XXEngine

XXEngine 是一个基于 C++ 的图形学项目，当前集成了两条渲染后端：

- 软光栅化（CPU）
- OpenGL（实时交互）

项目使用 ImGui 构建统一配置面板，在同一窗口中完成功能切换、模型切换、着色效果选择与结果预览。

## 功能概览

- 软光栅化功能
  - 贝塞尔曲线绘制
  - Blinn-Phong 相关着色流程（`texture / normal / phong / bump / displacement`）
  - 可选择模型并进行离屏渲染结果展示
- OpenGL 功能
  - 最小三角形回退渲染（MVP 验证）
  - 模型加载与显示（当前支持 OBJ 与 glTF）
  - 纹理显示与 Blinn-Phong shader 路径
  - 鼠标/键盘交互（相机平移、模型平移/旋转）
  - 工具栏支持一键重置与自动旋转开关

## 效果展示


<table>
  <tr>
    <td align="center" width="33%">
      <img src="readme_files/贝塞尔.gif" alt="贝塞尔曲线演示" width="100%" />
      <br/>
      <sub>图 1：贝塞尔曲线功能演示</sub>
    </td>
    <td align="center" width="33%">
      <img src="readme_files/软光栅化器.gif" alt="软光栅化器演示" width="100%" />
      <br/>
      <sub>图 2：软光栅化器渲染演示</sub>
    </td>
    <td align="center" width="33%">
      <img src="readme_files/OpenGL+blinnPhong.gif" alt="OpenGL Blinn-Phong 演示" width="100%" />
      <br/>
      <sub>图 3：OpenGL Blinn-Phong 演示</sub>
    </td>
  </tr>
</table>


<table>
  <tr>
    <td align="center" width="50%">
      <img src="readme_files/blinnphong对比图.png" alt="Blinn-Phong 对比图" width="100%" />
      <br/>
      <sub>图 4：Blinn-Phong 效果对比</sub>
    </td>
    <td align="center" width="50%">
      <img src="readme_files/bump对比图.png" alt="Bump 对比图" width="100%" />
      <br/>
      <sub>图 5：Bump 效果对比</sub>
    </td>
  </tr>
</table>

<table>
  <tr>
    <td align="center" width="50%">
      <img src="readme_files/MSAA对比图.png" alt="MSAA 对比图" width="100%" />
      <br/>
      <sub>图 6：MSAA 效果对比</sub>
    </td>
    <td align="center" width="50%">
      <img src="readme_files/贝塞尔曲线的抗锯齿对比图.png" alt="贝塞尔曲线抗锯齿对比图" width="100%" />
      <br/>
      <sub>图 7：贝塞尔曲线抗锯齿效果对比</sub>
    </td>
  </tr>
</table>

## 项目结构

```text
XXEngine/
├─ lib/                      # 第三方库（ImGui、stb 等）
├─ src/
│  ├─ bezier/                # 贝塞尔曲线相关
│  ├─ loaders/               # 模型加载抽象层（OBJ / glTF，可扩展）
│  ├─ models/                # 模型资源与 models.json
│  ├─ opengl/                # OpenGL 后端与 shader
│  ├─ rasterizer/            # 软光栅化核心代码
│  ├─ ConfigPanel.*          # 左侧配置面板
│  └─ main.cpp               # 程序入口与主循环
├─ XXEngine.sln
└─ XXEngine.vcxproj
```

## 依赖环境

当前工程以 Windows + Visual Studio 为主：

- Visual Studio 2022（MSVC v143）
- C++17
- OpenGL (`opengl32.lib`)
- GLFW 3.4（项目配置使用 Win64 预编译包）
- Eigen3
- OpenCV（用于读取 `models.json`，当前工程链接 `opencv_world4120d.lib`）
- tinygltf（已通过 include 路径接入）

说明：以上路径在 `XXEngine.vcxproj` 中按本机绝对路径配置，请按你的本地安装位置调整 `IncludePath / LibraryPath / AdditionalDependencies`。

## 构建与运行

1. 使用 Visual Studio 2022 打开 `XXEngine.sln`
2. 选择 `Debug | x64` 配置（当前依赖项配置最完整）
3. 构建并运行 `XXEngine` 项目

## 使用说明

程序启动后：

- 左侧 `Config` 面板
  - `Backend`：
    - `Raster`：软光栅化路径
    - `OpenGL`：OpenGL 路径
  - `Models`：模型选择（在 Raster/OpenGL 下均可用）
- 右侧渲染区域
  - Raster：显示软光栅化结果
  - OpenGL：显示实时渲染结果与工具栏

OpenGL 交互（当前逻辑）：

- 鼠标左键拖动：移动相机
- 鼠标右键拖动：旋转模型
- `W/A/S/D`：移动模型
- `Ctrl + W/A/S/D`：旋转模型
- `Ctrl + 鼠标移动`：旋转模型
- 工具栏按钮：
  - `Reset Camera/Model`：重置相机和模型位姿
  - `Auto Rotate`：自动旋转开关

## 模型配置（models.json）

模型列表由 `src/models/models.json` 驱动，关键字段：

- `id`：模型 ID
- `name`：显示名称
- `loaders`：加载器类型（如 `obj` / `gltf`）
- `modelpath`：模型路径（相对 `models.json`）
- `opengl_rotation_deg`：OpenGL 下模型初始旋转矫正（XYZ）
- `texturespath`：纹理路径数组（当前主流程使用第一个作为主纹理）

示例：

```json
{
  "id": 2,
  "name": "DamagedHelmet",
  "loaders": "gltf",
  "modelpath": "./DamagedHelmet/glTF/DamagedHelmet.gltf",
  "opengl_rotation_deg": [0.0, 0.0, 0.0],
  "texturespath": [
    "./DamagedHelmet/glTF/Default_albedo.jpg"
  ]
}
```

## 可扩展方向

- 在 `src/loaders` 新增更多格式加载器（如 FBX、更多 glTF 变体）
- 在 `src/opengl/shaders` 下扩展更多着色效果（如 PBR）
- 将 OpenGL 与 Raster 的材质/灯光参数统一为同一套配置结构

## 已知说明

- 当前工程主要在 Windows（`Debug | x64`）配置下验证。
- 如果遇到编译报错，优先检查第三方依赖路径和库名是否与本地环境一致。

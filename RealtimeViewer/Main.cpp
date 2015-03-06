#include "Windows/Window.h"
#include "Windows/Application.h"
#include "Core/Renderer.h"
#include "Graphics/Camera.h"
#include "Utils/Mesh.h"

#include "Graphics/EDXGui.h"
#include "Windows/Timer.h"

#include "Graphics/OpenGL.h"

using namespace EDX;
using namespace EDX::GUI;
using namespace EDX::RasterRenderer;

int giWindowWidth = 1280;
int giWindowHeight = 720;

int gTexFilterId = 2;
int gMSAAId = 0;
bool gHRas = true;
bool gRecord = false;

// Global variables
RefPtr<Renderer>		gpRenderer;
Camera			gCamera;
Mesh			gMesh;
Timer			gTimer;

void GUIEvent(Object* pObject, EventArgs args);

void OnInit(Object* pSender, EventArgs args)
{
	OpenGL::InitializeOpenGLExtensions();
	glClearColor(0.4f, 0.5f, 0.65f, 1.0f);

	gpRenderer = new Renderer;
	gpRenderer->Initialize(giWindowWidth, giWindowHeight);
	gCamera.Init(-5.0f * Vector3::UNIT_Z, Vector3::ZERO, Vector3::UNIT_Y, giWindowWidth, giWindowHeight, 65, 0.01f);

	//gMesh.LoadPlane(Vector3::ZERO, Vector3(1, 1, 1), Vector3(-90.0f, 0.0f, 0.0f), 1.2f);
	gMesh.LoadSphere(Vector3::ZERO, Vector3::UNIT_SCALE, Vector3::ZERO, 1.2f);
	//gMesh.LoadMesh(Vector3(0, -10, 35), Vector3::UNIT_SCALE, Vector3(0, 180, 0), "../../Media/bunny.obj");
	//gMesh.LoadMesh(Vector3(0, -10, 35), 0.003f * Vector3::UNIT_SCALE, Vector3(0, 180, 0), "../../Media/venusm.obj");
	//gMesh.LoadMesh(Vector3(0, 0, 35), Vector3::UNIT_SCALE, Vector3(180, 180, 0), "../../Media/budha.obj");
	//gMesh.LoadMesh(Vector3(0, 0, 35), Vector3::UNIT_SCALE, Vector3(0, 0, 0), "../../Media/teapot.obj");
	//gMesh.LoadMesh(Vector3(0, 0, 0), Vector3::UNIT_SCALE, Vector3(0, 180, 0), "../../Media/cornell-box/cornellbox.obj");
	//gMesh.LoadMesh(Vector3(0, 0, 0), 5.0f * Vector3::UNIT_SCALE, Vector3(0, -90, 0), "../../Media/dragon.obj");
	//gMesh.LoadMesh(Vector3(2, -2, -5), Vector3::UNIT_SCALE, Vector3(0, 0, 0), "../../Media/san-miguel/san-miguel.obj");
	//gMesh.LoadMesh(Vector3(0, 0, 0), 0.01f * Vector3::UNIT_SCALE, Vector3(0, 0, 0), "../../Media/sponza/sponza.obj");
	//gMesh.LoadMesh(Vector3(0, 0, 0), 0.01f * Vector3::UNIT_SCALE, Vector3(0, 0, 0), "../../Media/crytek-sponza/sponza.obj");

	Vector3 center;
	float radius;
	gMesh.GetBounds().BoundingSphere(&center, &radius);
	gCamera.mMoveScaler = radius / 50.0f;

	// Initialize UI
	EDXGui::Init();

	gTimer.Start();
}


void OnRender(Object* pSender, EventArgs args)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	gpRenderer->SetTransform(gCamera.GetViewMatrix(), gCamera.GetProjMatrix(), gCamera.GetRasterMatrix());
	gpRenderer->RenderMesh(gMesh);

	glRasterPos3f(0.0f, 0.0f, 1.0f);
	glDrawPixels(giWindowWidth, giWindowHeight, GL_RGBA, GL_UNSIGNED_BYTE, gpRenderer->GetBackBuffer());

	gTimer.MarkFrame();

	EDXGui::BeginFrame();
	EDXGui::BeginDialog(LayoutStrategy::DockRight);
	{
		EDXGui::Text("Image Res: %i, %i", giWindowWidth, giWindowHeight);
		EDXGui::Text("Triangle Count: %i", gMesh.GetIndexBuffer()->GetTriangleCount());
		EDXGui::Text(gTimer.GetFrameRate());

		EDXGui::CheckBox("Hierarchical Rasterize", gHRas);
		EDXGui::CheckBox("Record Frames", gRecord);

		ComboBoxItem AAItems[] = {
				{ 0, "off" },
				{ 1, "2x" },
				{ 2, "4x" },
				{ 3, "8x" },
				{ 4, "16x" }
		};
		EDXGui::ComboBox("MSAA:", AAItems, 5, gMSAAId);

		ComboBoxItem FilterItems[] = {
				{ 0, "Nearst" },
				{ 1, "Linear" },
				{ 2, "Trilinear" },
				{ 3, "4x Anisotropic" },
				{ 4, "8x Anisotropic" },
				{ 5, "16x Anisotropic" }
		};
		EDXGui::ComboBox("Texture Filter:", FilterItems, 6, gTexFilterId);

		if (EDXGui::Button("Load Scene"))
		{
			char filePath[MAX_PATH];
			char directory[MAX_PATH];
			sprintf_s(directory, MAX_PATH, "%s../../Media", Application::GetBaseDirectory());
			if (Application::GetMainWindow()->OpenFileDialog(directory, "obj", "Wavefront Object\0*.obj", filePath))
			{
				gMesh.Release();
				gMesh.LoadMesh(Vector3(0, 0, 0), 0.01f * Vector3::UNIT_SCALE, Vector3(0, 0, 0), filePath);

				Vector3 center;
				float radius;
				gMesh.GetBounds().BoundingSphere(&center, &radius);

				gCamera.Init(Vector3::ZERO, Vector3::UNIT_Z, Vector3::UNIT_Y, giWindowWidth, giWindowHeight, 65, radius * 0.02f, radius * 5.0f);
				gCamera.mMoveScaler = radius / 50.0f;
			}
		}
	}
	EDXGui::EndDialog();
	EDXGui::EndFrame();
}

void OnResize(Object* pSender, ResizeEventArgs args)
{
	glViewport(0, 0, args.Width, args.Height);

	// Set opengl params
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, args.Width, 0, args.Height, -1, 1);

	// 	Matrix mProj = gCamera.GetProjMatrix();
	// 	glLoadTransposeMatrixf((float*)&mProj);

	glMatrixMode(GL_MODELVIEW);

	gCamera.Resize(args.Width, args.Height);
	gpRenderer->Resize(args.Width, args.Height);

	giWindowWidth = args.Width;
	giWindowHeight = args.Height;

	EDXGui::Resize(args.Width, args.Height);
}

void OnRelease(Object* pSender, EventArgs args)
{
	gpRenderer.Dereference();
	gMesh.~Mesh();
	EDXGui::Release();
}

void OnMouseEvent(Object* pSender, MouseEventArgs args)
{
	EDXGui::HandleMouseEvent(args);

	gCamera.HandleMouseMsg(args);
}

void OnKeyboardEvent(Object* pSender, KeyboardEventArgs args)
{
	EDXGui::HandleKeyboardEvent(args);

	gCamera.HandleKeyboardMsg(args);
}


int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR cmdArgs, int cmdShow)
{
	Application::Init(hInst);

	Window* mainWindow = new GLWindow;
	mainWindow->SetMainLoop(NotifyEvent(OnRender));
	mainWindow->SetInit(NotifyEvent(OnInit));
	mainWindow->SetResize(ResizeEvent(OnResize));
	mainWindow->SetRelease(NotifyEvent(OnRelease));
	mainWindow->SetMouseHandler(MouseEvent(OnMouseEvent));
	mainWindow->SetkeyboardHandler(KeyboardEvent(OnKeyboardEvent));

	mainWindow->Create(L"EDXRaster", giWindowWidth, giWindowHeight);

	Application::Run(mainWindow);

	return 0;
}
#include "Windows/Window.h"
#include "Windows/Application.h"
#include "Core/Renderer.h"
#include "Graphics/Camera.h"
#include "Utils/Mesh.h"

#include "Graphics/EDXGui.h"
#include "Windows/Timer.h"

#include <gl/GL.h>

using namespace EDX;
using namespace EDX::GUI;
using namespace EDX::RasterRenderer;

int giWindowWidth = 800;
int giWindowHeight = 600;

int gTexFilterId = 2;
int gMSAAId = 0;

// Global variables
RefPtr<Renderer>		gRenderer;
Camera			gCamera;
Mesh			gMesh;
EDXDialog		gDialog;
Timer			gTimer;

void GUIEvent(uint iID, EDXControl* pControl);

void OnInit(Object* pSender, EventArgs args)
{
	glClearColor(0.4f, 0.5f, 0.65f, 1.0f);

	gRenderer = new Renderer;
	gRenderer->Initialize(giWindowWidth, giWindowHeight);
	gCamera.Init(-5.0f * Vector3::UNIT_Z, Vector3::ZERO, Vector3::UNIT_Y, giWindowWidth, giWindowHeight, 65);

	//gMesh.LoadPlane(Vector3::ZERO, Vector3(1, 1, 1), Vector3(-90.0f, 0.0f, 0.0f), 1.2f);
	gMesh.LoadSphere(Vector3::ZERO, Vector3::UNIT_SCALE, Vector3::ZERO, 0.8f);
	//gMesh.LoadMesh(Vector3(0, -10, 35), Vector3::UNIT_SCALE, Vector3(0, 180, 0), "../../Media/bunny.obj");
	//gMesh.LoadMesh(Vector3(0, -10, 35), 0.003f * Vector3::UNIT_SCALE, Vector3(0, 180, 0), "../../Media/venusm.obj");
	//gMesh.LoadMesh(Vector3(0, 0, 35), Vector3::UNIT_SCALE, Vector3(180, 180, 0), "../../Media/budha.obj");
	//gMesh.LoadMesh(Vector3(0, 0, 35), Vector3::UNIT_SCALE, Vector3(0, 0, 0), "../../Media/teapot.obj");
	//gMesh.LoadMesh(Vector3(0, 0, 0), Vector3::UNIT_SCALE, Vector3(0, 180, 0), "../../Media/cornell-box/cornellbox.obj");
	//gMesh.LoadMesh(Vector3(0, 0, 0), 5.0f * Vector3::UNIT_SCALE, Vector3(0, -90, 0), "../../Media/dragon.obj");
	//gMesh.LoadMesh(Vector3(2, -2, -5), Vector3::UNIT_SCALE, Vector3(0, 0, 0), "../../Media/san-miguel/san-miguel.obj");
	//gMesh.LoadMesh(Vector3(0, 0, 0), 0.01f * Vector3::UNIT_SCALE, Vector3(0, 0, 0), "../../Media/sponza/sponza.obj");
	//gMesh.LoadMesh(Vector3(0, 0, 0), 0.01f * Vector3::UNIT_SCALE, Vector3(0, 0, 0), "../../Media/crytek-sponza/sponza.obj");

	// Initialize UI
	gDialog.Init(giWindowWidth - 200, 0, giWindowWidth, giWindowHeight);
	gDialog.SetCallback(GUIEvent);

	int iY = 20;
	gDialog.AddText(0, 30, iY += 24, 100, 20, "Image Res: 800, 600");
	gDialog.AddText(1, 30, iY += 24, 100, 20, "FPS: 0");
	gDialog.AddText(2, 30, iY += 24, 100, 20, "");
	gDialog.AddText(3, 30, iY += 24, 100, 20, "");
	gDialog.AddButton(4, 30, iY += 24, 100, 20, "Load Scene");
	gDialog.AddButton(5, 30, iY += 24, 100, 20, "Toggle MSAA");
	gDialog.AddButton(6, 30, iY += 24, 100, 20, "Toggle Filter");

	gTimer.Start();
}


void OnRender(Object* pSender, EventArgs args)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	gRenderer->SetTransform(gCamera.GetViewMatrix(), gCamera.GetProjMatrix(), gCamera.GetRasterMatrix());
	gRenderer->RenderMesh(gMesh);

	glRasterPos3f(0.0f, 0.0f, 0.0f);
	glDrawPixels(giWindowWidth, giWindowHeight, GL_RGBA, GL_FLOAT, gRenderer->GetBackBuffer());

	gTimer.MarkFrame();

	Text* pText = (Text*)gDialog.GetControlWithID(0);
	char strText[_MAX_PATH] = { 0 };
	_snprintf_s(strText, _MAX_PATH, "Image Res: %i, %i\0", giWindowWidth, giWindowHeight);
	pText->SetText(strText);

	((Text*)gDialog.GetControlWithID(1))->SetText(gTimer.GetFrameRate());

	static char* msaaStr[] = {
		"MSAA: off",
		"MSAA: 4x",
		"MSAA: 8x",
		"MSAA: 16x"
	};
	((Text*)gDialog.GetControlWithID(2))->SetText(msaaStr[gMSAAId]);

	static char* filterStr[] = {
		"TextureFilter: Nearst",
		"TextureFilter: Linear",
		"TextureFilter: Trilinear",
		"TextureFilter: 4x Anisotropic",
		"TextureFilter: 8x Anisotropic",
		"TextureFilter: 16x Anisotropic"
	};
	((Text*)gDialog.GetControlWithID(3))->SetText(filterStr[gTexFilterId]);

	gDialog.Render();
	// 	Matrix mView = gCamera.GetViewMatrix();
	// 	glLoadTransposeMatrixf((float*)&mView);
	// 
	// 	glBindBuffer(GL_ARRAY_BUFFER, giVboId1);
	// 	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, giVboId2);
	// 
	// 	glEnableClientState(GL_VERTEX_ARRAY);
	// 	glVertexPointer(3, GL_FLOAT, sizeof(MeshVertex), 0);
	// 	glEnableClientState(GL_COLOR_ARRAY);
	// 	glColorPointer(3, GL_FLOAT, sizeof(MeshVertex), reinterpret_cast<void*>(offsetof(MeshVertex, nNormal)));
	// 	
	// 	glDrawElements(GL_TRIANGLES, 3 * gMesh.GetTriangleCount(), GL_UNSIGNED_INT, 0);
	// 
	// 	glDisableClientState(GL_VERTEX_ARRAY);
	// 
	// 	glBindBuffer(GL_ARRAY_BUFFER, 0);
	// 	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
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
	gRenderer->Resize(args.Width, args.Height);
	gDialog.Resize(args.Width, args.Height);

	giWindowWidth = args.Width;
	giWindowHeight = args.Height;
}

void OnRelease(Object* pSender, EventArgs args)
{
	gRenderer.Dereference();
	gMesh.~Mesh();
}

void OnMouseEvent(Object* pSender, MouseEventArgs args)
{
	if (gDialog.MsgProc(args))
		return;

	gCamera.HandleMouseMsg(args);
}

void OnKeyboardEvent(Object* pSender, KeyboardEventArgs args)
{
	gCamera.HandleKeyboardMsg(args);
}

void GUIEvent(uint iID, EDXControl* pControl)
{
	switch (iID)
	{
	case 4:
		OPENFILENAMEA ofn;
		ZeroMemory(&ofn, sizeof(ofn));

		char szFile[MAX_PATH];       // buffer for file name

		// Initialize OPENFILENAME
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = Application::GetMainWindow()->GetHandle();
		ofn.lpstrFile = szFile;
		ofn.lpstrFile[0] = '\0';
		ofn.nMaxFile = sizeof(szFile);
		ofn.lpstrFilter = "";
		ofn.nFilterIndex = 1;
		ofn.lpstrFileTitle = NULL;
		ofn.nMaxFileTitle = 0;
		ofn.lpstrInitialDir = "../../Media";
		ofn.lpstrDefExt = "obj";
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
		if (GetOpenFileNameA(&ofn) != 0)
		{
			gMesh.Release();
			gMesh.LoadMesh(Vector3(0, 0, 0), 0.01f * Vector3::UNIT_SCALE, Vector3(0, 0, 0), ofn.lpstrFile);
		}
		return;

	case 5:
		gMSAAId++;
		gMSAAId %= 4;
		gRenderer->SetMSAAMode(gMSAAId);
		return;

	case 6:
		gTexFilterId++;
		gTexFilterId %= 6;
		gRenderer->SetTextureFilter(TextureFilter(gTexFilterId));
		return;
	}
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
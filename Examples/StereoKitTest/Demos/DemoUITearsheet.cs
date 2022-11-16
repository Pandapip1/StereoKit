﻿using StereoKit;
using System;

class DemoUITearsheet : ITest
{
	string title       = "UI Tearsheet";
	string description = "An enumeration of all the different types of UI elements!";

	Sprite sprToggleOn  = Sprite.FromFile("toggle_on.png");
	Sprite sprToggleOff = Sprite.FromFile("toggle_off.png");
	Sprite sprSearch    = Sprite.FromFile("search.png");

	public void Initialize()
	{
	}

	public void Shutdown()
	{
	}

	int index = 1;
	void Unique(Action a) { UI.PushId(index); index += 1; a(); UI.PopId(); }

	Pose   buttonWindowPose = new Pose(0,0,0);
	bool[] toggles          = new bool[10];
	int    radio            = 0;
	void ShowButtonWindow()
	{
		UI.WindowBegin("Buttons", ref buttonWindowPose);

		Unique(() => UI.Button("UI.Button"));
		UI.SameLine();
		Unique(() => UI.Button("UI.Button", new Vec2(0.14f,0)));

		Unique(() => UI.ButtonImg("UI.ButtonImg", sprSearch, UIBtnLayout.Left));
		UI.SameLine();
		Unique(() => UI.ButtonImg("UI.ButtonImg", sprSearch, UIBtnLayout.Center));

		Unique(() => UI.ButtonImg("UI.ButtonImg", sprSearch, UIBtnLayout.Right));
		UI.SameLine();
		Unique(() => UI.ButtonImg("UI.ButtonImg", sprSearch, UIBtnLayout.CenterNoText));

		UI.Label("UI.ButtonRound");
		UI.SameLine();
		Unique(() => UI.ButtonRound("UI.ButtonRound", sprSearch));

		Unique(() => UI.Toggle("UI.Toggle", ref toggles[0]));
		UI.SameLine();
		Unique(() => UI.Toggle("UI.Toggle", ref toggles[1], new Vec2(0.14f,0)));

		Unique(() => UI.Toggle("UI.Toggle", ref toggles[2], sprToggleOff, sprToggleOn));
		UI.SameLine();
		Unique(() => UI.Toggle("UI.Toggle", ref toggles[3], sprToggleOff, sprToggleOn, UIBtnLayout.Left, new Vec2(0.14f, 0)));

		Unique(() => { if (UI.Radio("UI.Radio", radio == 0)) radio = 0; });
		UI.SameLine();
		Unique(() => { if (UI.Radio("UI.Radio", radio == 1)) radio = 1; });
		UI.SameLine();
		Unique(() => { if (UI.Radio("UI.Radio", radio == 2)) radio = 2; });

		UI.WindowEnd();
	}

	Pose   sliderWindowPose = new Pose(-0.3f, 0, 0);
	float  sliderValf = 0;
	double sliderVald = 0;
	void ShowSliderWindow()
	{
		UI.WindowBegin("Slides & Separators", ref sliderWindowPose);

		Unique(() => UI.HSlider("UI.HSlider", ref sliderValf, 0, 1, 0, 0, UIConfirm.Push));
		Unique(() => UI.HSlider("UI.HSlider", ref sliderVald, 0, 1, 0, 0, UIConfirm.Pinch));
		UI.HSeparator();
		UI.ProgressBar((Time.Totalf%3.0f)/3.0f);

		UI.WindowEnd();
	}

	Pose   textWindowPose = new Pose(-0.6f, 0, 0);
	string textInput = "Text here...";
	void ShowTextWindow()
	{
		UI.WindowBegin("Text", ref textWindowPose, V.XY(0.25f,0));

		UI.Label("UI.Label", true);
		UI.SameLine();
		UI.Label("UI.Label", new Vec2(0.14f,0));
		UI.Label("UI.Label", false);

		UI.HSeparator();

		UI.Text("UI.Text", TextAlign.TopLeft);
		UI.Text("UI.Text", TextAlign.TopCenter);
		UI.Text("UI.Text", TextAlign.TopRight);

		UI.HSeparator();

		UI.Label("UI.Input");
		UI.SameLine();
		UI.Input("Input", ref textInput);

		UI.WindowEnd();
	}

	public void Update()
	{
		index = 0;
		Hierarchy.Push(Demo.contentPose);
		ShowButtonWindow();
		ShowSliderWindow();
		ShowTextWindow();
		Hierarchy.Pop();
		Demo.ShowSummary(title, description);
	}
}
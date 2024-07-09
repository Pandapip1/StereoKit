﻿// SPDX-License-Identifier: MIT
// The authors below grant copyright rights under the MIT license:
// Copyright (c) 2019-2023 Nick Klingensmith
// Copyright (c) 2023 Qualcomm Technologies, Inc.

using StereoKit;
using System.Collections.Generic;

class DemoEyes : ITest
{
	string title       = "Eye Tracking";
	string description = "If the hardware supports it, and permissions are granted, eye tracking is as simple as grabbing Input.Eyes!\n\nThis scene is raycasting your eye ray at the indicated plane, and the dot's red/green color indicates eye tracking availability! On flatscreen you can simulate eye tracking with Alt+Mouse.";

	List<LinePoint> points = new List<LinePoint>();
	Vec3 previous;

	long   lastEyesSampleTime;
	double demoStartTime;
	int    uniqueSamplesCount;

	public void Initialize()
	{
		demoStartTime      = Time.Total;
		uniqueSamplesCount = 0;
		lastEyesSampleTime = -1;
	}
	public void Shutdown  () { }

	public void Step()
	{
		Matrix quadPose = Matrix.S(0.4f) * Demo.contentPose;
		Plane  plane    = new Plane(quadPose * Vec3.Zero, quadPose.TransformNormal(Vec3.Forward));
		Mesh.Quad.Draw(Material.Default, quadPose);
		if (Input.Eyes.Ray.Intersect(plane, out Vec3 at))
		{
			Color stateColor = Input.EyesTracked.IsActive() 
				? new Color(0,1,0)
				: new Color(1,0,0);
			Default.MeshSphere.Draw(Default.Material, Matrix.TS(at, 3*U.cm), stateColor);
			if (Vec3.DistanceSq(at, previous) > U.cm*U.cm) {
				previous = at;
				points.Add(new LinePoint { pt = at, color = Color.White });
				if (points.Count > 20)
					points.RemoveAt(0);
			}

			LinePoint pt = points[points.Count - 1];
			pt.pt = at;
			points[points.Count - 1] = pt;
		}

		for (int i = 0; i < points.Count; i++) { 
			LinePoint pt = points[i];
			pt.thickness = (i / (float)points.Count) * 3 * U.cm;
			points[i] = pt;
		}

		Lines.Add(points.ToArray());

		if (Backend.XRType == BackendXRType.OpenXR && Device.HasEyeGaze)
		{
			if (Backend.OpenXR.EyesSampleTime != lastEyesSampleTime)
			{
				lastEyesSampleTime = Backend.OpenXR.EyesSampleTime;
				uniqueSamplesCount++;
			}

			double sampleFrequency = uniqueSamplesCount / (Time.Total - demoStartTime);
			Text.Add($"Eye tracker sampling frequency: {sampleFrequency:0.#} Hz", Matrix.T(V.XYZ(0, -0.75f, -0.1f)) * quadPose);
		}

		Demo.ShowSummary(title, description, new Bounds(.44f, .44f, 0.1f));
	}
}
﻿using StereoKit;

class TestMatrixShaderParam : ITest
{
	Material _material;

	public void Initialize()
	{
		_material = new Material(Shader.FromFile("matrix_param.hlsl"));
		_material[MatParamName.DiffuseTex] = Tex.FromFile("test.png");
		_material["custom_transform"] = Matrix.TS(-0.25f, 0, 0, 0.25f).Transposed;
	}

	public void Shutdown() { }

	public void Step()
	{
		Mesh.Cube.Draw(_material, Matrix.Identity);

		Tests.Screenshot("Tests/MatrixParam.jpg", 600, 600, 90, V.XYZ(0, 0, .5f), V.XYZ(0, 0, 0));
	}
}
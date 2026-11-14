#pragma once

#include <cmath>

namespace pt
{
	// Small 3D vector for the engine-free cave code. Units are centimetres and
	// the axes match Unreal's (X forward, Y right, Z up), so values pass
	// straight into FVector.
	struct Vec3
	{
		double X = 0.0;
		double Y = 0.0;
		double Z = 0.0;

		constexpr Vec3() = default;
		constexpr Vec3(double InX, double InY, double InZ) : X(InX), Y(InY), Z(InZ) {}

		constexpr Vec3 operator+(const Vec3& O) const { return Vec3(X + O.X, Y + O.Y, Z + O.Z); }
		constexpr Vec3 operator-(const Vec3& O) const { return Vec3(X - O.X, Y - O.Y, Z - O.Z); }
		constexpr Vec3 operator-() const { return Vec3(-X, -Y, -Z); }
		constexpr Vec3 operator*(double S) const { return Vec3(X * S, Y * S, Z * S); }
		constexpr Vec3 operator/(double S) const { return Vec3(X / S, Y / S, Z / S); }
		Vec3& operator+=(const Vec3& O) { X += O.X; Y += O.Y; Z += O.Z; return *this; }
		Vec3& operator-=(const Vec3& O) { X -= O.X; Y -= O.Y; Z -= O.Z; return *this; }
		Vec3& operator*=(double S) { X *= S; Y *= S; Z *= S; return *this; }

		constexpr double Dot(const Vec3& O) const { return X * O.X + Y * O.Y + Z * O.Z; }
		constexpr Vec3 Cross(const Vec3& O) const { return Vec3(Y * O.Z - Z * O.Y, Z * O.X - X * O.Z, X * O.Y - Y * O.X); }
		constexpr double LengthSq() const { return Dot(*this); }
		double Length() const { return std::sqrt(LengthSq()); }
		constexpr Vec3 Mul(const Vec3& O) const { return Vec3(X * O.X, Y * O.Y, Z * O.Z); }
		constexpr Vec3 Div(const Vec3& O) const { return Vec3(X / O.X, Y / O.Y, Z / O.Z); }

		Vec3 Normalized() const
		{
			const double Len = Length();
			return Len > 1e-12 ? *this / Len : Vec3();
		}

		static constexpr Vec3 Lerp(const Vec3& A, const Vec3& B, double T) { return A + (B - A) * T; }
		static Vec3 Min(const Vec3& A, const Vec3& B) { return Vec3(std::fmin(A.X, B.X), std::fmin(A.Y, B.Y), std::fmin(A.Z, B.Z)); }
		static Vec3 Max(const Vec3& A, const Vec3& B) { return Vec3(std::fmax(A.X, B.X), std::fmax(A.Y, B.Y), std::fmax(A.Z, B.Z)); }
	};

	inline constexpr Vec3 operator*(double S, const Vec3& V) { return V * S; }

	// Axis-aligned box.
	struct Box3
	{
		Vec3 Min;
		Vec3 Max;

		constexpr bool Overlaps(const Box3& O) const
		{
			return Min.X <= O.Max.X && Max.X >= O.Min.X && Min.Y <= O.Max.Y && Max.Y >= O.Min.Y
				&& Min.Z <= O.Max.Z && Max.Z >= O.Min.Z;
		}

		constexpr bool Contains(const Vec3& P) const
		{
			return P.X >= Min.X && P.X <= Max.X && P.Y >= Min.Y && P.Y <= Max.Y && P.Z >= Min.Z && P.Z <= Max.Z;
		}

		Box3 Expanded(double By) const { return Box3{Min - Vec3(By, By, By), Max + Vec3(By, By, By)}; }
	};

	// Distance from P to the segment AB, and the parameter (0..1) of the closest point.
	inline double SegmentDistance(const Vec3& P, const Vec3& A, const Vec3& B, double* OutT = nullptr)
	{
		const Vec3 AB = B - A;
		const double Len2 = AB.LengthSq();
		double T = Len2 > 1e-12 ? (P - A).Dot(AB) / Len2 : 0.0;
		T = T < 0.0 ? 0.0 : (T > 1.0 ? 1.0 : T);
		if (OutT)
		{
			*OutT = T;
		}
		return (P - (A + AB * T)).Length();
	}
}

#include "PaleotrenchCore/CaveField.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "PaleotrenchCore/Random.h"
#include "PaleotrenchCore/Seed.h"

namespace pt
{
	namespace
	{
		constexpr double DeepRock = 1.0e6;
		constexpr size_t MaxStackFeatures = 256;

		double Fade(double T)
		{
			return T * T * T * (T * (T * 6.0 - 15.0) + 10.0);
		}

		double LerpD(double T, double A, double B)
		{
			return A + T * (B - A);
		}

		double Grad(int Hash, double X, double Y, double Z)
		{
			const int H = Hash & 15;
			const double U = H < 8 ? X : Y;
			const double V = H < 4 ? Y : (H == 12 || H == 14 ? X : Z);
			return ((H & 1) == 0 ? U : -U) + ((H & 2) == 0 ? V : -V);
		}

		// Polynomial smooth minimum: blends two distances over K.
		double SmoothMin(double A, double B, double K)
		{
			const double H = std::fmax(K - std::fabs(A - B), 0.0) / K;
			return std::fmin(A, B) - H * H * K * 0.25;
		}

		double EllipsoidDistance(const Vec3& Q, const Vec3& Radii)
		{
			const double K0 = Q.Div(Radii).Length();
			const double K1 = Q.Div(Radii.Mul(Radii)).Length();
			if (K1 < 1e-12)
			{
				return -std::fmin(Radii.X, std::fmin(Radii.Y, Radii.Z));
			}
			return K0 * (K0 - 1.0) / K1;
		}
	}

	GradientNoise::GradientNoise(uint64_t Seed)
	{
		std::array<uint8_t, 256> Base{};
		for (size_t i = 0; i < Base.size(); ++i)
		{
			Base[i] = static_cast<uint8_t>(i);
		}
		Rng R(Seed, 0x7E15ULL);
		R.Shuffle(Base.data(), Base.size());
		for (size_t i = 0; i < Perm.size(); ++i)
		{
			Perm[i] = Base[i & 255u];
		}
	}

	double GradientNoise::Sample(const Vec3& P) const
	{
		const double Fx = std::floor(P.X);
		const double Fy = std::floor(P.Y);
		const double Fz = std::floor(P.Z);
		const int Xi = static_cast<int>(static_cast<int64_t>(Fx) & 255);
		const int Yi = static_cast<int>(static_cast<int64_t>(Fy) & 255);
		const int Zi = static_cast<int>(static_cast<int64_t>(Fz) & 255);
		const double X = P.X - Fx;
		const double Y = P.Y - Fy;
		const double Z = P.Z - Fz;
		const double U = Fade(X);
		const double V = Fade(Y);
		const double W = Fade(Z);
		const auto At = [this](int Index) { return static_cast<int>(Perm[static_cast<size_t>(Index)]); };
		const int A = At(Xi) + Yi;
		const int AA = At(A) + Zi;
		const int AB = At(A + 1) + Zi;
		const int B = At(Xi + 1) + Yi;
		const int BA = At(B) + Zi;
		const int BB = At(B + 1) + Zi;
		return LerpD(W,
			LerpD(V, LerpD(U, Grad(At(AA), X, Y, Z), Grad(At(BA), X - 1.0, Y, Z)),
				LerpD(U, Grad(At(AB), X, Y - 1.0, Z), Grad(At(BB), X - 1.0, Y - 1.0, Z))),
			LerpD(V, LerpD(U, Grad(At(AA + 1), X, Y, Z - 1.0), Grad(At(BA + 1), X - 1.0, Y, Z - 1.0)),
				LerpD(U, Grad(At(AB + 1), X, Y - 1.0, Z - 1.0), Grad(At(BB + 1), X - 1.0, Y - 1.0, Z - 1.0))));
	}

	double GradientNoise::Fractal(const Vec3& P, int Octaves) const
	{
		double Sum = 0.0;
		double Amp = 1.0;
		double Freq = 1.0;
		double Norm = 0.0;
		for (int o = 0; o < Octaves; ++o)
		{
			// Each octave is shifted so lattice points never line up.
			Sum += Amp * Sample(P * Freq + Vec3(17.31 * o, 41.77 * o, 9.13 * o));
			Norm += Amp;
			Amp *= 0.5;
			Freq *= 2.0;
		}
		return Norm > 0.0 ? Sum / Norm : 0.0;
	}

	CaveField::CaveField(const CaveGraph& InGraph, const CaveFieldTuning& InTuning)
		: Layout(InGraph)
		, Settings(InTuning)
		, Noise(MixSeed(InGraph.Seed, 0x40C4ULL))
	{
		const double Reach = Margin() + 10.0;
		const double NoiseMax = Settings.LumpStrength + Settings.DetailStrength;
		for (size_t i = 0; i < Layout.Nodes.size(); ++i)
		{
			const CaveNode& Node = Layout.Nodes[i];
			Feature Chamber;
			Chamber.Kind = EFeature::Chamber;
			Chamber.A = Node.Centre;
			Chamber.Radii = Node.Radii;
			Chamber.FloorZ = Node.FloorZ();
			Chamber.NoiseCap = std::fmin(NoiseMax, Settings.ChamberNoiseShare * std::fmin(Node.Radii.Z, std::fmin(Node.Radii.X, Node.Radii.Y)) * 1.6);
			Chamber.Box = Box3{Node.Centre - Node.Radii, Node.Centre + Node.Radii}.Expanded(Reach);
			Features.push_back(Chamber);
			if (Node.HasAir())
			{
				// A dome in the ceiling holds the air; the water line sits just under the ceiling's top.
				CaveAirDome Dome;
				Dome.Node = static_cast<int>(i);
				Dome.Radius = Settings.DomeRadius;
				Dome.Height = Settings.DomeHeight;
				Dome.WaterCentre = Vec3(Node.Centre.X, Node.Centre.Y, Node.CeilingZ() - Settings.DomeHeight * 0.2);
				Domes.push_back(Dome);
				Feature Cap;
				Cap.Kind = EFeature::Dome;
				Cap.A = Dome.WaterCentre;
				Cap.Radii = Vec3(Dome.Radius, Dome.Radius, Dome.Height);
				Cap.FloorZ = -std::numeric_limits<double>::max();
				Cap.NoiseCap = std::fmin(NoiseMax, 0.08 * Dome.Radius);
				Cap.Box = Box3{Cap.A - Cap.Radii, Cap.A + Cap.Radii}.Expanded(Reach);
				Features.push_back(Cap);
			}
		}
		for (const CavePassage& Passage : Layout.Passages)
		{
			for (size_t s = 0; s + 1 < Passage.Path.size(); ++s)
			{
				Feature Segment;
				Segment.Kind = EFeature::Segment;
				Segment.A = Passage.Path[s];
				Segment.B = Passage.Path[s + 1];
				Segment.Radius = Passage.Radius;
				Segment.NoiseCap = std::fmin(NoiseMax, Settings.PassageNoiseShare * Passage.Radius);
				const Vec3 Pad(Passage.Radius, Passage.Radius, Passage.Radius);
				Segment.Box = Box3{Vec3::Min(Segment.A, Segment.B) - Pad, Vec3::Max(Segment.A, Segment.B) + Pad}.Expanded(Reach);
				Features.push_back(Segment);
			}
		}

		const double Big = std::numeric_limits<double>::max();
		OpenBounds = Box3{Vec3(Big, Big, Big), Vec3(-Big, -Big, -Big)};
		for (const Feature& F : Features)
		{
			OpenBounds.Min = Vec3::Min(OpenBounds.Min, F.Box.Min);
			OpenBounds.Max = Vec3::Max(OpenBounds.Max, F.Box.Max);
		}
		GridOrigin = OpenBounds.Min;
		GridX = std::max(1, static_cast<int>(std::ceil((OpenBounds.Max.X - OpenBounds.Min.X) / GridCell)));
		GridY = std::max(1, static_cast<int>(std::ceil((OpenBounds.Max.Y - OpenBounds.Min.Y) / GridCell)));
		GridZ = std::max(1, static_cast<int>(std::ceil((OpenBounds.Max.Z - OpenBounds.Min.Z) / GridCell)));
		Grid.assign(static_cast<size_t>(GridX) * static_cast<size_t>(GridY) * static_cast<size_t>(GridZ), {});
		for (size_t f = 0; f < Features.size(); ++f)
		{
			const Box3& B = Features[f].Box;
			const int X0 = std::clamp(static_cast<int>((B.Min.X - GridOrigin.X) / GridCell), 0, GridX - 1);
			const int X1 = std::clamp(static_cast<int>((B.Max.X - GridOrigin.X) / GridCell), 0, GridX - 1);
			const int Y0 = std::clamp(static_cast<int>((B.Min.Y - GridOrigin.Y) / GridCell), 0, GridY - 1);
			const int Y1 = std::clamp(static_cast<int>((B.Max.Y - GridOrigin.Y) / GridCell), 0, GridY - 1);
			const int Z0 = std::clamp(static_cast<int>((B.Min.Z - GridOrigin.Z) / GridCell), 0, GridZ - 1);
			const int Z1 = std::clamp(static_cast<int>((B.Max.Z - GridOrigin.Z) / GridCell), 0, GridZ - 1);
			for (int z = Z0; z <= Z1; ++z)
			{
				for (int y = Y0; y <= Y1; ++y)
				{
					for (int x = X0; x <= X1; ++x)
					{
						const size_t Cell = (static_cast<size_t>(z) * static_cast<size_t>(GridY) + static_cast<size_t>(y)) * static_cast<size_t>(GridX)
							+ static_cast<size_t>(x);
						Grid[Cell].push_back(static_cast<int>(f));
					}
				}
			}
		}
	}

	double CaveField::Margin() const
	{
		return Settings.BlendRadius + Settings.LumpStrength + Settings.DetailStrength;
	}

	std::vector<int> CaveField::FeaturesNear(const Box3& Box) const
	{
		std::vector<int> Found;
		if (!Box.Overlaps(OpenBounds))
		{
			return Found;
		}
		const int X0 = std::clamp(static_cast<int>((Box.Min.X - GridOrigin.X) / GridCell), 0, GridX - 1);
		const int X1 = std::clamp(static_cast<int>((Box.Max.X - GridOrigin.X) / GridCell), 0, GridX - 1);
		const int Y0 = std::clamp(static_cast<int>((Box.Min.Y - GridOrigin.Y) / GridCell), 0, GridY - 1);
		const int Y1 = std::clamp(static_cast<int>((Box.Max.Y - GridOrigin.Y) / GridCell), 0, GridY - 1);
		const int Z0 = std::clamp(static_cast<int>((Box.Min.Z - GridOrigin.Z) / GridCell), 0, GridZ - 1);
		const int Z1 = std::clamp(static_cast<int>((Box.Max.Z - GridOrigin.Z) / GridCell), 0, GridZ - 1);
		for (int z = Z0; z <= Z1; ++z)
		{
			for (int y = Y0; y <= Y1; ++y)
			{
				for (int x = X0; x <= X1; ++x)
				{
					const size_t Cell = (static_cast<size_t>(z) * static_cast<size_t>(GridY) + static_cast<size_t>(y)) * static_cast<size_t>(GridX)
						+ static_cast<size_t>(x);
					for (const int f : Grid[Cell])
					{
						if (Features[static_cast<size_t>(f)].Box.Overlaps(Box))
						{
							Found.push_back(f);
						}
					}
				}
			}
		}
		std::sort(Found.begin(), Found.end());
		Found.erase(std::unique(Found.begin(), Found.end()), Found.end());
		return Found;
	}

	double CaveField::FeatureDistance(const Feature& F, const Vec3& P) const
	{
		switch (F.Kind)
		{
		case EFeature::Chamber:
			return std::fmax(EllipsoidDistance(P - F.A, F.Radii), F.FloorZ - P.Z);
		case EFeature::Dome:
			return EllipsoidDistance(P - F.A, F.Radii);
		case EFeature::Segment:
			return SegmentDistance(P, F.A, F.B) - F.Radius;
		}
		return DeepRock;
	}

	double CaveField::Sample(const Vec3& P) const
	{
		return SampleWith(P, FeaturesNear(Box3{P, P}));
	}

	double CaveField::SampleWith(const Vec3& P, const std::vector<int>& Nearby) const
	{
		if (Nearby.empty())
		{
			return DeepRock;
		}
		double Stack[MaxStackFeatures];
		std::vector<double> Heap;
		double* Dist = Stack;
		if (Nearby.size() > MaxStackFeatures)
		{
			Heap.resize(Nearby.size());
			Dist = Heap.data();
		}
		double Hard = DeepRock;
		for (size_t i = 0; i < Nearby.size(); ++i)
		{
			Dist[i] = FeatureDistance(Features[static_cast<size_t>(Nearby[i])], P);
			Hard = std::fmin(Hard, Dist[i]);
		}
		// Blend only pieces within the blend radius of the nearest one, in a fixed order, so leaving out a
		// distant piece (as chunk-local feature lists do) never changes the value near the surface.
		const double K = Settings.BlendRadius;
		double Value = DeepRock;
		bool bFirst = true;
		double CapSum = 0.0;
		double WeightSum = 0.0;
		for (size_t i = 0; i < Nearby.size(); ++i)
		{
			if (Dist[i] >= Hard + K)
			{
				continue;
			}
			Value = bFirst ? Dist[i] : SmoothMin(Value, Dist[i], K);
			bFirst = false;
			const double Closeness = 1.0 - (Dist[i] - Hard) / K;
			const double Weight = Closeness * Closeness;
			CapSum += Weight * Features[static_cast<size_t>(Nearby[i])].NoiseCap;
			WeightSum += Weight;
		}
		if (Hard > Margin() * 2.0)
		{
			return Value;  // deep in the rock: noise cannot bring the surface here
		}
		const double Cap = WeightSum > 0.0 ? CapSum / WeightSum : 0.0;
		const double Total = Settings.LumpStrength + Settings.DetailStrength;
		const double Lumps = Noise.Fractal(P / Settings.LumpSize, 3) * (Settings.LumpStrength / Total);
		const double Detail = Noise.Fractal(P / Settings.DetailSize + Vec3(91.7, 13.3, 57.1), 2) * (Settings.DetailStrength / Total);
		return Value + Cap * (Lumps + Detail);
	}

	Vec3 CaveField::Gradient(const Vec3& P, const std::vector<int>& Nearby, double Step) const
	{
		const double Dx = SampleWith(P + Vec3(Step, 0.0, 0.0), Nearby) - SampleWith(P - Vec3(Step, 0.0, 0.0), Nearby);
		const double Dy = SampleWith(P + Vec3(0.0, Step, 0.0), Nearby) - SampleWith(P - Vec3(0.0, Step, 0.0), Nearby);
		const double Dz = SampleWith(P + Vec3(0.0, 0.0, Step), Nearby) - SampleWith(P - Vec3(0.0, 0.0, Step), Nearby);
		return Vec3(Dx, Dy, Dz) / (2.0 * Step);
	}
}

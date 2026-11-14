#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "PaleotrenchCore/Api.h"
#include "PaleotrenchCore/CaveGraph.h"
#include "PaleotrenchCore/Vec3.h"

namespace pt
{
	// Seeded 3D gradient noise (improved Perlin). Same seed, same values everywhere.
	// (Member functions are exported one by one: exporting a class with std members warns on MSVC.)
	class GradientNoise
	{
	public:
		PT_API explicit GradientNoise(uint64_t Seed = 0);
		PT_API double Sample(const Vec3& P) const;  // roughly -1..1
		PT_API double Fractal(const Vec3& P, int Octaves) const;  // octaves of halving size and strength, about -1..1

	private:
		std::array<uint8_t, 512> Perm{};
	};

	// Step 2 of cave generation: the rock as a signed distance field. Negative is open water (or air in
	// the domes of air pockets), positive is rock, and the surface is where it crosses zero. Chambers are
	// ellipsoids with flat silt floors, passages are tubes along their paths, air pockets add a dome to
	// the chamber ceiling. The pieces are blended smoothly and the walls get noise whose strength is
	// limited by the size of the passage, so noise can never close a squeeze.
	struct CaveFieldTuning
	{
		double BlendRadius = 260.0;       // how softly passages and chambers merge, cm
		double LumpStrength = 150.0;      // large rock lumps, cm
		double LumpSize = 1200.0;         // their size, cm
		double DetailStrength = 35.0;     // small surface detail, cm
		double DetailSize = 280.0;
		double ChamberNoiseShare = 0.16;  // at most this share of a chamber's height
		double PassageNoiseShare = 0.32;  // at most this share of a passage's radius
		double DomeRadius = 380.0;        // air pocket dome, cm (fits the air pocket actor's 2 x 2 m air box)
		double DomeHeight = 330.0;
	};

	// One air pocket in the generated rock: the dome's water line, centre and size. The game puts an air
	// pocket actor here.
	struct CaveAirDome
	{
		int Node = -1;
		Vec3 WaterCentre;   // centre of the water surface under the dome
		double Radius = 0.0;
		double Height = 0.0;
	};

	class CaveField
	{
	public:
		PT_API CaveField(const CaveGraph& InGraph, const CaveFieldTuning& InTuning = CaveFieldTuning());

		// Signed distance-like value at P (cm, cave space).
		PT_API double Sample(const Vec3& P) const;
		// The same, considering only the Nearby features (from FeaturesNear); much faster for many samples.
		PT_API double SampleWith(const Vec3& P, const std::vector<int>& Nearby) const;
		// Direction of steepest increase (into the rock); its negation points into the open.
		PT_API Vec3 Gradient(const Vec3& P, const std::vector<int>& Nearby, double Step) const;

		// Features whose influence reaches Box (which is open somewhere only if this is non-empty).
		PT_API std::vector<int> FeaturesNear(const Box3& Box) const;
		// Box around all open space, with room for noise.
		const Box3& Bounds() const { return OpenBounds; }
		const std::vector<CaveAirDome>& AirDomes() const { return Domes; }
		const CaveGraph& GetGraph() const { return Layout; }
		// Largest distance the surface can move away from the plain shapes (noise and blending).
		PT_API double Margin() const;

	private:
		enum class EFeature : uint8_t { Chamber, Segment, Dome };
		struct Feature
		{
			EFeature Kind = EFeature::Chamber;
			Vec3 A;          // chamber or dome centre, or segment start
			Vec3 B;          // segment end
			Vec3 Radii;      // chamber or dome half-sizes
			double Radius = 0.0;   // segment radius
			double FloorZ = 0.0;   // chamber floor (-huge for none)
			double NoiseCap = 0.0; // the most noise allowed near this feature
			Box3 Box;              // influence box
		};

		double FeatureDistance(const Feature& F, const Vec3& P) const;

		CaveGraph Layout;
		CaveFieldTuning Settings;
		GradientNoise Noise;
		std::vector<Feature> Features;
		std::vector<CaveAirDome> Domes;
		Box3 OpenBounds;
		// Uniform grid of feature indices for fast lookups.
		Vec3 GridOrigin;
		double GridCell = 2000.0;
		int GridX = 1;
		int GridY = 1;
		int GridZ = 1;
		std::vector<std::vector<int>> Grid;
	};
}

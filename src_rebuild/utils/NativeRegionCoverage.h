#ifndef NATIVE_REGION_COVERAGE_H
#define NATIVE_REGION_COVERAGE_H
#include <array>
#include <bitset>
#include <cstdint>
#include <limits>

namespace NativeAsset
{
// Packed CELL_DATA references use 14-bit object slots. Slots are only frame-local
// accounting keys: logical picking keeps its separate copied placement identity.
struct RegionCoverageCounts
{
	unsigned int selected = 0, copied = 0, copiedParts = 0;
	unsigned int viewInside = 0, viewOutside = 0, viewUncopied = 0;
	unsigned int duplicateView = 0, invalid = 0;
	unsigned int published = 0, publishedParts = 0;
};
class RegionCoverage
{
public:
	static const unsigned int Capacity = 16384;
	void BeginFrame()
	{
		m_selected.reset(); m_view.reset(); m_published.reset();
		m_parts.fill(0); m_counts = RegionCoverageCounts();
	}
	bool Select(unsigned int slot)
	{
		if (!Valid(slot)) return false;
		if (m_selected.test(slot)) return false; // repeated cell reference
		m_selected.set(slot); ++m_counts.selected; return true;
	}
	bool Copy(unsigned int slot, unsigned int parts)
	{
		if (!Valid(slot)) return false;
		if (!m_selected.test(slot) || m_parts[slot] || !parts ||
			parts > std::numeric_limits<uint16_t>::max())
		{ ++m_counts.invalid; return false; }
		m_parts[slot] = uint16_t(parts); ++m_counts.copied;
		m_counts.copiedParts += parts; return true;
	}
	void ObserveView(unsigned int slot)
	{
		if (!Valid(slot)) return;
		if (m_view.test(slot)) { ++m_counts.duplicateView; return; }
		m_view.set(slot);
		if (!m_selected.test(slot)) { ++m_counts.viewOutside; return; }
		++m_counts.viewInside;
		if (!m_parts[slot]) ++m_counts.viewUncopied;
	}
	void BeginPublication()
	{
		m_published.reset(); m_counts.published = m_counts.publishedParts = 0;
	}
	bool Publish(unsigned int slot, unsigned int parts)
	{
		if (!Valid(slot)) return false;
		if (!m_parts[slot] || m_published.test(slot) || parts != m_parts[slot])
		{ ++m_counts.invalid; return false; }
		m_published.set(slot); ++m_counts.published;
		m_counts.publishedParts += parts; return true;
	}
	bool CompletePublication() const
	{
		return !m_counts.invalid && m_counts.selected == m_counts.copied &&
			m_counts.copied == m_counts.published &&
			m_counts.copiedParts == m_counts.publishedParts;
	}
	const RegionCoverageCounts& Counts() const { return m_counts; }
private:
	bool Valid(unsigned int slot)
	{
		if (slot < Capacity) return true;
		++m_counts.invalid; return false;
	}
	std::bitset<Capacity> m_selected, m_view, m_published;
	std::array<uint16_t, Capacity> m_parts = {};
	RegionCoverageCounts m_counts;
};
}
#endif

#pragma once

#include <Qt>

// Pure index math for arrow-key navigation over the DCA mapping panel's
// channel/bus grids. Items run top-to-bottom in blocks of rowsPerBlock, blocks
// left-to-right (channel grid: 16 rows, bus grid: 8), so item i sits at
// (row i % rowsPerBlock, block i / rowsPerBlock).
//
// Header-only and GUI-free so it links into headless unit tests.
namespace OpenMix::DcaGridNav {

// the index reached by moving (dRow, dCol) from index, or -1 when the move
// leaves the grid
[[nodiscard]] inline int move(int index, int count, int rowsPerBlock, int dRow, int dCol) {
    if (index < 0 || index >= count || rowsPerBlock <= 0)
        return -1;
    const int row = index % rowsPerBlock;
    const int block = index / rowsPerBlock;
    const int newRow = row + dRow;
    const int newBlock = block + dCol;
    if (newRow < 0 || newRow >= rowsPerBlock || newBlock < 0)
        return -1;
    const int target = newBlock * rowsPerBlock + newRow;
    return (target >= 0 && target < count) ? target : -1;
}

// combo row for a digit/clear key: 0 (None) for 0/Delete/Backspace, 1..dcaCount
// for digit keys, -1 for anything else or out-of-range digits
[[nodiscard]] inline int comboRowForKey(int key, int dcaCount) {
    if (key == Qt::Key_0 || key == Qt::Key_Delete || key == Qt::Key_Backspace)
        return 0;
    if (key >= Qt::Key_1 && key <= Qt::Key_9) {
        const int dca = key - Qt::Key_0;
        return dca <= dcaCount ? dca : -1;
    }
    return -1;
}

} // namespace OpenMix::DcaGridNav

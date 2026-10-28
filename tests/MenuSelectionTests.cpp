#include "MenuSelection.hpp"

#include <iostream>
#include <stdexcept>

namespace {

void expect(const arrow::MenuSelection& selection, int index, const char* message) {
    if (selection.selected() != index) throw std::runtime_error(message);
}

void checkMenu(int count) {
    const auto enabled = [=](int index) {
        return index >= 0 && index < count && (count != 4 || index != 1);
    };
    arrow::MenuSelection selection;
    selection.updatePointer(-10, -10, std::nullopt, true);
    selection.move(1, count, enabled);
    const int bookmark = count == 4 ? 2 : 1;
    expect(selection, bookmark, "Down did not skip disabled rows");

    selection.updatePointer(100, 100, count - 1, true);
    expect(selection, count - 1, "Hover did not override the keyboard bookmark");
    selection.updatePointer(100, 101, count - 1, true);
    expect(selection, count - 1, "Movement within the row changed selection");
    selection.updatePointer(20, 20, std::nullopt, true);
    expect(selection, bookmark, "Leaving rows did not restore keyboard selection");

    selection.updatePointer(100, 100, count - 1, true);
    selection.move(1, count, enabled);
    expect(selection, 0, "Down did not start from hover and wrap");
    selection.updatePointer(100, 100, count - 1, true);
    expect(selection, 0, "Stationary mouse stole keyboard selection");
    selection.move(1, count, enabled);
    selection.updatePointer(100, 100, count - 1, true);
    expect(selection, bookmark, "Repeated keyboard navigation started from hover");
    selection.updatePointer(101, 100, count - 1, true);
    expect(selection, count - 1, "New pointer movement did not reclaim selection");
    selection.updatePointer(20, 20, std::nullopt, true);
    expect(selection, bookmark, "Hover replaced the updated keyboard bookmark");

    selection.updatePointer(100, 100, 0, true);
    selection.move(-1, count, enabled);
    expect(selection, count - 1, "Up did not start from hover and wrap");
    selection.updatePointer(100, 100, 0, true);
    selection.move(-1, count, enabled);
    expect(selection, count - 2, "Up restarted from stationary hover");

    // Clicking a setting changes its value, not the keyboard bookmark.
    selection.updatePointer(100, 100, 0, true);
    selection.usePointer();
    expect(selection, 0, "Click did not select the row under a stationary pointer");
    selection.updatePointer(20, 20, std::nullopt, true);
    expect(selection, count - 2, "Click discarded the keyboard bookmark");

    // Enter/Space activate the visible row and give control to the keyboard.
    selection.updatePointer(100, 100, count - 1, true);
    selection.useKeyboard();
    expect(selection, count - 1, "Keyboard activation selected an invisible row");
    selection.updatePointer(20, 20, std::nullopt, true);
    expect(selection, count - 1, "Keyboard activation did not keep its selected row");

    selection.updatePointer(100, 100, 0, false);
    expect(selection, count - 1, "Inactive window used hover");
    selection.updatePointer(100, 100, 0, true);
    expect(selection, 0, "Pointer did not resume on window reentry");

    selection.reset();
    selection.validate(count, enabled);
    expect(selection, 0, "Page change did not reset keyboard selection");
    selection.updatePointer(100, 100, count - 1, true);
    expect(selection, count - 1, "New page ignored a stationary pointer");
}

void checkAvailabilityChanges() {
    arrow::MenuSelection selection;
    const auto all = [](int index) { return index >= 0 && index < 4; };
    selection.move(1, 4, all);
    selection.updatePointer(100, 100, 1, true);
    const auto onlySettings = [](int index) { return index == 2; };
    selection.validate(4, onlySettings);
    expect(selection, 2, "Disabled keyboard and hovered rows remained selected");
    selection.move(1, 4, onlySettings);
    expect(selection, 2, "Navigation failed with a single enabled row");
    selection.move(-1, 4, onlySettings);
    expect(selection, 2, "Reverse navigation failed with a single enabled row");
    selection.validate(2, [](int index) { return index == 0; });
    expect(selection, 0, "Selection remained outside a smaller menu");
}

} // namespace

int main() {
    try {
        checkMenu(4);
        checkMenu(5);
        checkMenu(6);
        checkAvailabilityChanges();
        std::cout << "Menu selection tests passed: hover/bookmark, keyboard takeover, "
                     "stationary pointer, click/activation, wrap, disabled rows, focus and page changes.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Menu selection test failure: " << error.what() << '\n';
        return 1;
    }
}

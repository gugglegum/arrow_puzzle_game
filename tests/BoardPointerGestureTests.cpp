#include "BoardPointerGesture.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void expectMovement(arrow::BoardPointerGesture::Movement movement, float x, float y) {
    expect(std::abs(movement.x - x) < 0.0001F && std::abs(movement.y - y) < 0.0001F,
           "Drag did not follow the pointer in screen coordinates");
}

void checkClicks() {
    arrow::BoardPointerGesture gesture;
    expect(!gesture.release(7), "Release without a board press launched an arrow");
    gesture.begin(100, 200, 7);
    expect(gesture.release(7) == 7, "Stationary click did not launch its arrow");
    expect(!gesture.release(7), "One click launched twice");

    gesture.begin(100, 200, 7);
    expectMovement(gesture.move(103, 204), 0, 0);
    expect(gesture.release(7) == 7, "Touchpad jitter cancelled a click");
    gesture.begin(100, 200, 7);
    expectMovement(gesture.move(106, 200), 0, 0);
    expect(gesture.release(7) == 7, "Movement at the threshold cancelled a click");

    gesture.begin(100, 200, 7);
    expect(!gesture.release(8), "Release over another arrow launched an arrow");
    gesture.begin(100, 200, 7);
    expect(!gesture.release(std::nullopt), "Release outside an arrow launched it");
    gesture.begin(100, 200, std::nullopt);
    expect(!gesture.release(7), "A press on empty space armed an arrow");
}

void checkDrags() {
    arrow::BoardPointerGesture gesture;
    gesture.begin(100, 200, 7);
    expectMovement(gesture.move(103, 200), 0, 0);
    expectMovement(gesture.move(107, 200), 7, 0);
    expectMovement(gesture.move(120, 215), 13, 15);
    expect(!gesture.release(7), "Dragging an arrow launched it on release");

    gesture.begin(100, 200, 7);
    expectMovement(gesture.move(105, 205), 5, 5);
    expectMovement(gesture.move(100, 200), -5, -5);
    expect(!gesture.release(7), "Returning to the drag origin rearmed the arrow");

    gesture.begin(100, 200, std::nullopt);
    expectMovement(gesture.move(80, 170), -20, -30);
    expect(!gesture.release(7), "Dragging from empty space launched an arrow");

    // The controller feeds the release position through move() as well, even
    // when the first large displacement arrives together with button release.
    gesture.begin(100, 200, 7);
    expectMovement(gesture.move(200, 200), 100, 0);
    expect(!gesture.release(7), "A drag completed in the release frame launched an arrow");
    expectMovement(gesture.move(250, 200), 0, 0);

    gesture.begin(100, 200, 7);
    expect(gesture.release(7) == 7, "A completed drag blocked the next ordinary click");
}

void checkCancellation() {
    arrow::BoardPointerGesture gesture;
    gesture.begin(100, 200, 7);
    gesture.cancelLaunch();
    expect(!gesture.release(7), "A cancelled launch was restored before release");

    gesture.begin(100, 200, 7);
    gesture.cancelLaunch();
    expectMovement(gesture.move(120, 200), 20, 0);
    expect(!gesture.release(7), "Camera navigation armed an arrow");

    gesture.begin(100, 200, 7);
    gesture.reset();
    expectMovement(gesture.move(150, 200), 0, 0);
    expect(!gesture.release(7), "Focus loss or a screen change left a pending click");

    gesture.begin(100, 200, 7);
    expectMovement(gesture.move(120, 200), 20, 0);
    gesture.reset();
    gesture.begin(100, 200, 8);
    expect(gesture.release(8) == 8, "A new board press inherited the old drag");
}

} // namespace

int main() {
    try {
        checkClicks();
        checkDrags();
        checkCancellation();
        std::cout << "Board pointer tests passed: clicks, jitter, threshold, arrow/empty-space "
                     "drags, return to origin, release movement and cancellation.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Board pointer test failure: " << error.what() << '\n';
        return 1;
    }
}

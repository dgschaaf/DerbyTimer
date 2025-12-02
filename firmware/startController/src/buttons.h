#ifndef BUTTONS_H
#define BUTTONS_H

/**
 * @brief Configuration for the button inputs.
 *
 * Left is D18, Right is D19, Start is A6, Mode is A7
 * 
 */

// Setup/teardown
void setupButtons();

// Public API
bool isStartPressed();
bool isModePressed();
bool isLeftPressed();
bool isRightPressed();

#endif  // BUTTONS_H
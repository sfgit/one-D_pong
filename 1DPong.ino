#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

#define PIN_BUTTON_LEFT 2
#define PIN_BUTTON_RIGHT 3
#define PIN_LED_ENABLE_N A3

#define LED_COUNT 24

#define LED_OFF 0xFF
#define LED_DIM 0x7F
#define LED_ON  0

class LedDisplay {
public:
  LedDisplay(unsigned char size);
  ~LedDisplay();

  unsigned char size(void) const { return mSize; };
  void setLed(unsigned char index, unsigned char value);
  void clear(void);
  void show(void);
private:
  Adafruit_PWMServoDriver mPwmDriver0;
  Adafruit_PWMServoDriver mPwmDriver1;
  unsigned char* mpLed;
  unsigned char mSize;
};

LedDisplay::LedDisplay(unsigned char size)
: mPwmDriver0(0x40)
, mPwmDriver1(0x41)
{
  mSize = size;
  mpLed = new unsigned char[size];
  clear();

  //
  pinMode(PIN_LED_ENABLE_N, OUTPUT);
  digitalWrite(PIN_LED_ENABLE_N, HIGH);

  // Setup PWM drivers
  mPwmDriver0.begin();
  mPwmDriver1.begin();
  mPwmDriver0.setPWMFreq(1600);  // This is the maximum PWM frequency
  mPwmDriver1.setPWMFreq(1600);  // This is the maximum PWM frequency
 
  // save I2C bitrate
  uint8_t twbrbackup = TWBR;
  // must be changed after calling Wire.begin() (inside pwm.begin())
  TWBR = 12; // upgrade to 400KHz!
}

LedDisplay::~LedDisplay() {
  delete [] mpLed;
}

void LedDisplay::setLed(unsigned char index, unsigned char value) {
  if (index < mSize) {
    mpLed[index] = value;
  }
}

void LedDisplay::clear(void) {
  for (unsigned char i = 0; i < mSize; ++i) {
    mpLed[i] = LED_OFF;
  }
}

void LedDisplay::show(void) {
  for (unsigned char i = 0; i < mSize; ++i) {
    unsigned short value = mpLed[i];
    value <<= 4;
    // Extend the LSB
    if (value & 0x0010) {
      value |= 0x000f;
    }
    if (i < 12) {
      mPwmDriver0.setPWM(i, 0, value);
    } else if (i < 16) {
      mPwmDriver1.setPWM(i - 12, 0, value);
    } else if (i < 24) {
      mPwmDriver1.setPWM(i - 8, 0, value);      
    }
  }
  digitalWrite(PIN_LED_ENABLE_N, LOW);
}

#define GAME_MODE_POWERON 0
#define GAME_MODE_SELECT_GAME 1
#define GAME_MODE_ONE_PLAYER 2
#define GAME_MODE_TWO_PLAYER 3
#define GAME_MODE_PLAYER_LOSE 4
#define GAME_MODE_PLAYER_WIN 5
#define GAME_MODE_PLAYER_LEFT_WIN 6
#define GAME_MODE_PLAYER_RIGHT_WIN 7

#define GAME_FRAME_MS_MAX 20
#define GAME_FRAME_MS_MIN 10
#define GAME_FRAME_MS_START 20
#define GAME_FRAME_MS_STEP 1

#define GAME_BALL_DIRECTION_LEFT 1   // Ball moving to the left
#define GAME_BALL_DIRECTION_RIGHT 2  // Ball moving to the right
#define GAME_BALL_DIRECTION_ANY 3    // Ball moving either way

#define GAME_BALL_POSITION_LEFT 0
#define GAME_BALL_POSITION_RIGHT 23
#define GAME_WIN_LEFT_LED_MIN 2
#define GAME_WIN_LEFT_LED_MAX 11
#define GAME_WIN_RIGHT_LED_MIN 12
#define GAME_WIN_RIGHT_LED_MAX 21

#define GAME_LEFT_PADDLE_POSITION 2
#define GAME_LEFT_HIT_POSITION_MIN 2
#define GAME_LEFT_HIT_POSITION_MAX 4
#define GAME_LEFT_HIT_DIRECTION GAME_BALL_DIRECTION_LEFT
#define GAME_LEFT_HIT_RESET_POSITION 21
#define GAME_LEFT_HIT_RESET_DIRECTION GAME_DIRECTION_LEFT

#define GAME_RIGHT_PADDLE_POSITION 21
#define GAME_RIGHT_HIT_POSITION_MIN 19
#define GAME_RIGHT_HIT_POSITION_MAX 21
#define GAME_RIGHT_HIT_DIRECTION GAME_BALL_DIRECTION_RIGHT
#define GAME_RIGHT_HIT_RESET_POSITION 21
#define GAME_RIGHT_HIT_RESET_DIRECTION GAME_DIRECTION_RIGHT

#define GAME_PRESS_ENABLE 0
#define GAME_PRESS_PRESSED 1

#define GAME_ONE_PLAYER_LOSE_SCORE 0
#define GAME_ONE_PLAYER_WIN_SCORE 9
#define GAME_TWO_PLAYER_WIN_SCORE 9

class Game {
public:
  Game();
  void setMode(unsigned char mode);
  void frame(void);
protected:
  void powerOnFrame(void);
  void selectGameFrame(void);
  void onePlayerFrame(void);
  void twoPlayerFrame(void);
  void playerLoseFrame(void);
  void playerWinFrame(void);
  void playerLeftWinFrame(void);
  void playerRightWinFrame(void);
  void gameFrame(void);
  
  bool checkLeftButton(bool enable = true);
  bool checkRightButton(bool enable = true);
  void displayScores(void);
  void handleBallAtLeftPaddle(void);
  void handleBallAtRightPaddle(void);
  void handleBallAtLeftEnd(void);
  void handleBallAtRightEnd(void);
  
  void speedUp(void);
  void slowDown(void);
private:
  LedDisplay mDisplay;
  unsigned char mMode;
  int mFrameMilliseconds;
  unsigned char mBallPosition;
  unsigned char mBallDirection;
  unsigned char mLeftPress;
  unsigned char mRightPress;
  unsigned char mLeftScore;
  unsigned char mRightScore;
};

Game::Game()
  : mDisplay(LED_COUNT)
{
  setMode(GAME_MODE_POWERON);
}

void Game::setMode(unsigned char mode) {
  mMode = mode;
  mFrameMilliseconds = GAME_FRAME_MS_START;
  mBallPosition = (mode == GAME_MODE_TWO_PLAYER)
    ? GAME_RIGHT_PADDLE_POSITION
    : GAME_LEFT_PADDLE_POSITION;
  mBallDirection = (mode == GAME_MODE_TWO_PLAYER)
    ? GAME_BALL_DIRECTION_LEFT
    : GAME_BALL_DIRECTION_RIGHT;
  mLeftPress = (mode == GAME_MODE_TWO_PLAYER)
    ? GAME_PRESS_ENABLE
    : GAME_PRESS_PRESSED;
  mRightPress = (mode == GAME_MODE_ONE_PLAYER)
    ? GAME_PRESS_ENABLE
    : GAME_PRESS_PRESSED;
  mLeftScore = (mode == GAME_MODE_ONE_PLAYER) ? 3 : 0;
  mRightScore = 0;
}

void Game::frame(void) {
  mDisplay.clear();
  displayScores();

  switch (mMode) {
  case GAME_MODE_POWERON:
    powerOnFrame();
    break;
  case GAME_MODE_SELECT_GAME:
    selectGameFrame();
    break;
  case GAME_MODE_ONE_PLAYER:
    onePlayerFrame();
    break;
  case GAME_MODE_TWO_PLAYER:
    twoPlayerFrame();
    break;
  case GAME_MODE_PLAYER_LOSE:
    playerLoseFrame();
    break;
  case GAME_MODE_PLAYER_WIN:
    playerWinFrame();
    break;
  case GAME_MODE_PLAYER_LEFT_WIN:
    playerLeftWinFrame();
    break;
  case GAME_MODE_PLAYER_RIGHT_WIN:
    playerRightWinFrame();
    break;
  default:
    setMode(GAME_MODE_POWERON);
  }

  mDisplay.show();
  delay(mFrameMilliseconds);
}

void Game::powerOnFrame(void) {
  if (mBallPosition <= GAME_BALL_POSITION_RIGHT) {
  mDisplay.setLed(mBallPosition, LED_ON);
    mDisplay.setLed(GAME_BALL_POSITION_RIGHT - mBallPosition, LED_ON);
  } else {
    setMode(GAME_MODE_SELECT_GAME);
  }
  if (++mBallPosition > GAME_BALL_POSITION_RIGHT + 16) {
    mBallPosition = GAME_BALL_POSITION_RIGHT + 1;
  }
}

void Game::selectGameFrame(void) {
  if (mBallPosition < 8) {
      mDisplay.setLed(0, LED_ON);    
  } else {
      mDisplay.setLed(23, LED_ON);
      mDisplay.setLed(19, LED_ON);    
  }
  if (++mBallPosition >= 16) {
    mBallPosition = 0;
  }
  if (checkLeftButton()) {
    setMode(GAME_MODE_ONE_PLAYER);
  } else if (checkRightButton()) {
    setMode(GAME_MODE_TWO_PLAYER);
  }
}

void Game::onePlayerFrame(void) {
  gameFrame();
}

void Game::twoPlayerFrame(void) {
  gameFrame();
}

void Game::gameFrame(void) {
  // Display the paddles
  mDisplay.setLed(GAME_LEFT_PADDLE_POSITION, LED_DIM);
  mDisplay.setLed(GAME_RIGHT_PADDLE_POSITION, LED_DIM);

  // Display the ball at the current location
  mDisplay.setLed(mBallPosition, LED_ON);
  
  // Handle events
  if (mBallPosition >= GAME_LEFT_HIT_POSITION_MIN
    && mBallPosition <= GAME_LEFT_HIT_POSITION_MAX
    && mBallDirection == GAME_LEFT_HIT_DIRECTION) {      
    handleBallAtLeftPaddle();
  } else if (mBallPosition >= GAME_RIGHT_HIT_POSITION_MIN
    && mBallPosition <= GAME_RIGHT_HIT_POSITION_MAX
    && mBallDirection == GAME_RIGHT_HIT_DIRECTION) {
    handleBallAtRightPaddle();
  } else if (mBallPosition == GAME_BALL_POSITION_LEFT
     && mBallDirection == GAME_LEFT_HIT_DIRECTION) {
    handleBallAtLeftEnd();
    mBallDirection = GAME_BALL_DIRECTION_RIGHT;
  } else if (mBallPosition == GAME_BALL_POSITION_RIGHT
    && mBallDirection == GAME_RIGHT_HIT_DIRECTION) {
    handleBallAtRightEnd();
    mBallDirection = GAME_BALL_DIRECTION_LEFT;
  } else {
    checkLeftButton(false);
    checkRightButton(false);
  }
  
  // Advance the ball
  if (mBallDirection == GAME_BALL_DIRECTION_RIGHT) {
    ++mBallPosition;
  } else {
    --mBallPosition;
  }
}

void Game::playerLoseFrame(void) {
  mDisplay.setLed(GAME_LEFT_PADDLE_POSITION, LED_ON);
  mDisplay.setLed(GAME_RIGHT_PADDLE_POSITION, LED_ON);
  if (mBallPosition & 0x3) {
    mDisplay.setLed(GAME_LEFT_PADDLE_POSITION - 1, LED_ON);
    mDisplay.setLed(GAME_LEFT_PADDLE_POSITION + 1, LED_ON);
    mDisplay.setLed(GAME_RIGHT_PADDLE_POSITION - 1, LED_ON);
    mDisplay.setLed(GAME_RIGHT_PADDLE_POSITION + 1, LED_ON);
  }
  if (mBallPosition & 0x2 == 0x2) {
    mDisplay.setLed(GAME_LEFT_PADDLE_POSITION - 2, LED_ON);
    mDisplay.setLed(GAME_LEFT_PADDLE_POSITION + 2, LED_ON);
    mDisplay.setLed(GAME_RIGHT_PADDLE_POSITION - 2, LED_ON);
    mDisplay.setLed(GAME_RIGHT_PADDLE_POSITION + 2, LED_ON);
  }
  if (checkLeftButton() || checkRightButton()) {
    setMode(GAME_MODE_SELECT_GAME);
  }
  ++mBallPosition;
  mBallPosition &= 0x3;
}

void Game::playerWinFrame(void) {
  for (unsigned int i = 0; i < mBallPosition; ++i) {
    mDisplay.setLed(GAME_WIN_LEFT_LED_MAX - i, LED_ON);
    mDisplay.setLed(GAME_WIN_RIGHT_LED_MIN + i, LED_ON);
  }
  if (checkLeftButton() || checkRightButton()) {
    setMode(GAME_MODE_SELECT_GAME);
  }
  if (++mBallPosition > GAME_WIN_LEFT_LED_MAX - GAME_WIN_LEFT_LED_MIN) {
    mBallPosition = 0;
  }
}

void Game::playerLeftWinFrame(void) {
  for (unsigned int i = 0; i < mBallPosition; ++i) {
    mDisplay.setLed(GAME_WIN_LEFT_LED_MAX - i, LED_ON);
  }
  if (checkLeftButton() || checkRightButton()) {
    setMode(GAME_MODE_SELECT_GAME);
  }
  if (++mBallPosition > GAME_WIN_LEFT_LED_MAX - GAME_WIN_LEFT_LED_MIN) {
    mBallPosition = 0;
  }
}

void Game::playerRightWinFrame(void) {
  for (unsigned int i = 0; i < mBallPosition; ++i) {
    mDisplay.setLed(GAME_WIN_RIGHT_LED_MIN + i, LED_ON);
  }
  if (checkLeftButton() || checkRightButton()) {
    setMode(GAME_MODE_SELECT_GAME);
  }
  if (++mBallPosition > GAME_WIN_RIGHT_LED_MAX - GAME_WIN_RIGHT_LED_MIN) {
    mBallPosition = 0;
  }
}

void Game::handleBallAtLeftPaddle(void) {
  // Reenable the right button
  mRightPress = GAME_PRESS_ENABLE;
  if (checkLeftButton(false)) {
    // Left button hit at the proper time
    if (mMode == GAME_MODE_ONE_PLAYER) {
      if (++mLeftScore == GAME_ONE_PLAYER_WIN_SCORE) {
        setMode(GAME_MODE_PLAYER_WIN);
      }
    }
    mBallDirection = GAME_BALL_DIRECTION_RIGHT;
    speedUp();
  }
}

void Game::handleBallAtRightPaddle(void) {
  // Reenable the right button
  mLeftPress = GAME_PRESS_ENABLE;
  if (checkRightButton(false)) {
    // Right button hit at the proper time
    if (mMode == GAME_MODE_ONE_PLAYER) {
      if (++mLeftScore == GAME_ONE_PLAYER_WIN_SCORE) {
        setMode(GAME_MODE_PLAYER_WIN);
      }
    }
    mBallDirection = GAME_BALL_DIRECTION_LEFT;
    speedUp();
  }
}

void Game::handleBallAtLeftEnd(void) {
  // Reenable the right button
  mRightPress = GAME_PRESS_ENABLE;
  if (mMode == GAME_MODE_ONE_PLAYER) {
    // Reduce the score by one
    if (--mLeftScore == GAME_ONE_PLAYER_LOSE_SCORE) {
      setMode(GAME_MODE_PLAYER_LOSE);
    }
  } else if (mMode == GAME_MODE_TWO_PLAYER) {
    // Increase the right player's score
    if (++mRightScore == GAME_TWO_PLAYER_WIN_SCORE) {
      setMode(GAME_MODE_PLAYER_RIGHT_WIN);
    }
  }
  slowDown();
}

void Game::handleBallAtRightEnd(void) {
  // Reenable the right button
  mLeftPress = GAME_PRESS_ENABLE;
  if (mMode == GAME_MODE_ONE_PLAYER) {
    // Reduce the score by one
    if (--mLeftScore == GAME_ONE_PLAYER_LOSE_SCORE) {
      setMode(GAME_MODE_PLAYER_LOSE);
    }
  } else if (mMode == GAME_MODE_TWO_PLAYER) {
    // Increase the left player's score
    if (++mLeftScore == GAME_TWO_PLAYER_WIN_SCORE) {
      setMode(GAME_MODE_PLAYER_LEFT_WIN);
    }
  }
  slowDown();
}

void Game::speedUp(void) {
  mFrameMilliseconds -= GAME_FRAME_MS_STEP;
  if (mFrameMilliseconds < GAME_FRAME_MS_MIN) {
    mFrameMilliseconds = GAME_FRAME_MS_MIN;
  }
}

void Game::slowDown(void) {
  mFrameMilliseconds += GAME_FRAME_MS_STEP;
  if (mFrameMilliseconds > GAME_FRAME_MS_MAX) {
    mFrameMilliseconds = GAME_FRAME_MS_MAX;
  }
}

bool Game::checkLeftButton(bool enable) {
  if (mLeftPress == GAME_PRESS_PRESSED) {
    if (enable && digitalRead(PIN_BUTTON_LEFT) == LOW) {
      mLeftPress = GAME_PRESS_ENABLE;
    }
    return false;
  } else if (enable) {
    mLeftPress = GAME_PRESS_ENABLE;
  }
  if (mLeftPress == GAME_PRESS_ENABLE) {
    if (digitalRead(PIN_BUTTON_LEFT) == HIGH) {
      mLeftPress = GAME_PRESS_PRESSED;
      return true;
    }
  }
  return false;
}

bool Game::checkRightButton(bool enable) {
  if (mRightPress == GAME_PRESS_PRESSED) {
    if (enable && digitalRead(PIN_BUTTON_RIGHT) == LOW) {
      mRightPress = GAME_PRESS_ENABLE;
    }
    return false;
  } else if (enable) {
    mRightPress = GAME_PRESS_ENABLE;
  }
  if (mRightPress == GAME_PRESS_ENABLE) {
    if (digitalRead(PIN_BUTTON_RIGHT) == HIGH) {
      mRightPress = GAME_PRESS_PRESSED;
      return true;
    }
  }
  return false;
}

void Game::displayScores(void) {
  unsigned int i;
  for (i = 0; i < mLeftScore; ++i) {
    mDisplay.setLed(3 + i, LED_DIM);
  }
  for (i = 0; i < mRightScore; ++i) {
    mDisplay.setLed(20 - i, LED_DIM);
  }
}

//----------------------------------------------
void setup()
{
  pinMode(PIN_BUTTON_LEFT, INPUT);
  digitalWrite(PIN_BUTTON_LEFT, LOW);
  pinMode(PIN_BUTTON_RIGHT, INPUT);
  digitalWrite(PIN_BUTTON_RIGHT, LOW);
}

void loop()
{
  static Game game;
  
  game.frame(); 
}

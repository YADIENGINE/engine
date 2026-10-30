#pragma once

struct ButtonAnimSettings {
    float highlightStiffness = 0.0f; // zero selects immediately
    float highlightDamping = 0.0f;
    float maxStep = 1.0f / 30.0f;
    float pressSeconds = 0.0f; // zero disables feedback
    float denySeconds = 0.0f;
};

struct ButtonAnimState {
    float highlight = 0.0f;
    float press = 0.0f;
    float shake = 0.0f;
};

class ButtonAnim {
public:
    explicit ButtonAnim(const ButtonAnimSettings& settings = {});
    void Update(bool selected, float deltaSeconds);
    void Press();
    void Deny();
    void Clear();
    [[nodiscard]] ButtonAnimState State() const;

private:
    ButtonAnimSettings _settings;
    float _highlight = 0.0f;
    float _velocity = 0.0f;
    float _press = 0.0f;
    float _shake = 0.0f;
};

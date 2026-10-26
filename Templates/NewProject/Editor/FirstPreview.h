#pragma once
#include "Editor/Api/EditorPreview.h"
#include "Editor/Api/PreviewRegistry.h"

class FirstPreview final : public EditorPreview {
public:
    explicit FirstPreview(const PreviewParameters& parameters);
    [[nodiscard]] std::string ApplyParameter(const std::string& id, const PreviewValue& value) override;

private:
    void Draw() const override;
    float _radius;
};

[[nodiscard]] PreviewDefinition FirstPreviewDefinition();

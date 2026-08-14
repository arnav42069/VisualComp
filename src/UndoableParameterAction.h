#pragma once
#include "UndoRedoManager.h"
#include <JuceHeader.h>

namespace VisualCompUndo
{
//==============================================================================
// UndoableParameterAction: Undo/redo for APVTS parameter changes
//==============================================================================
class UndoableParameterAction : public UndoableAction
{
public:
    // Constructor without explicit oldValue: captures current parameter value
    UndoableParameterAction(juce::AudioProcessorValueTreeState& apvts,
                           const juce::String& paramID,
                           float newValue)
        : apvts(apvts), paramID(paramID), newValue(newValue)
    {
        if (auto param = apvts.getParameter(paramID))
            oldValue = param->getValue();
    }

    // Constructor with explicit oldValue: use when old value is known (e.g., captured on mouse-down)
    UndoableParameterAction(juce::AudioProcessorValueTreeState& apvts,
                           const juce::String& paramID,
                           float newValue,
                           float oldValue)
        : apvts(apvts), paramID(paramID), newValue(newValue), oldValue(oldValue)
    {
    }

    void execute() override
    {
        if (auto param = apvts.getParameter(paramID))
            param->setValueNotifyingHost(newValue);
    }

    void undo() override
    {
        if (auto param = apvts.getParameter(paramID))
            param->setValueNotifyingHost(oldValue);
    }

    juce::String getActionName() const override
    {
        return "Adjust " + paramID;
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::String paramID;
    float newValue, oldValue = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UndoableParameterAction)
};

//==============================================================================
// UndoableEqNodeEdit: Undo/redo for EQ node parameter changes
//==============================================================================
class UndoableEqNodeEdit : public UndoableAction
{
public:
    // Constructor without explicit oldState: captures current node state
    UndoableEqNodeEdit(ParametricEq& eq, int nodeIndex,
                      const EqNodeState& newState)
        : eq(eq), nodeIndex(nodeIndex), newState(newState)
    {
        oldState = eq.getNode(nodeIndex);
    }

    // Constructor with explicit oldState: use when old state is known (e.g., captured on mouse-down)
    UndoableEqNodeEdit(ParametricEq& eq, int nodeIndex,
                      const EqNodeState& newState,
                      const EqNodeState& oldState)
        : eq(eq), nodeIndex(nodeIndex), newState(newState), oldState(oldState)
    {
    }

    void execute() override
    {
        eq.setNode(nodeIndex, newState);
    }

    void undo() override
    {
        eq.setNode(nodeIndex, oldState);
    }

    juce::String getActionName() const override
    {
        return "Edit EQ Node " + juce::String(nodeIndex + 1);
    }

private:
    ParametricEq& eq;
    int nodeIndex;
    EqNodeState oldState, newState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UndoableEqNodeEdit)
};

//==============================================================================
// UndoableCompressorModeChange: Undo/redo for compressor mode switches
//==============================================================================
class UndoableCompressorModeChange : public UndoableAction
{
public:
    UndoableCompressorModeChange(VisualCompProcessor& processor,
                                int oldMode, int newMode)
        : processor(processor), oldMode(oldMode), newMode(newMode)
    {
    }

    void execute() override
    {
        processor.compMode.store(newMode, std::memory_order_release);
    }

    void undo() override
    {
        processor.compMode.store(oldMode, std::memory_order_release);
    }

    juce::String getActionName() const override
    {
        static const char* names[] = { "VCA", "FET", "Optical", "Tube" };
        return juce::String("Change Mode to ") + names[juce::jlimit(0, 3, newMode)];
    }

private:
    VisualCompProcessor& processor;
    int oldMode, newMode;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UndoableCompressorModeChange)
};

} // namespace VisualCompUndo

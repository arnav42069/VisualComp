#pragma once
#include <JuceHeader.h>
#include <memory>
#include <vector>

namespace VisualCompUndo
{
//==============================================================================
// UndoableAction: Base class for all undoable operations
// (Scoped in namespace to avoid conflict with juce::UndoableAction)
//==============================================================================
class UndoableAction
{
public:
    virtual ~UndoableAction() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
    virtual juce::String getActionName() const { return "Action"; }
};

//==============================================================================
// UndoRedoManager: Handles undo/redo stack and state management
//==============================================================================
class UndoRedoManager
{
public:
    UndoRedoManager(int maxStackSize = 100) : maxUndoSteps(maxStackSize) {}

    void perform(std::unique_ptr<UndoableAction> action)
    {
        if (!action) return;

        // Execute the action
        action->execute();

        // Add to undo stack and clear any redo stack
        undoStack.push_back(std::move(action));
        redoStack.clear();

        // Limit stack size
        if (undoStack.size() > size_t(maxUndoSteps))
            undoStack.erase(undoStack.begin());

        onStackChanged();
    }

    void undo()
    {
        if (undoStack.empty()) return;

        auto action = std::move(undoStack.back());
        undoStack.pop_back();

        action->undo();
        redoStack.push_back(std::move(action));

        onStackChanged();
    }

    void redo()
    {
        if (redoStack.empty()) return;

        auto action = std::move(redoStack.back());
        redoStack.pop_back();

        action->execute();
        undoStack.push_back(std::move(action));

        onStackChanged();
    }

    bool canUndo() const { return !undoStack.empty(); }
    bool canRedo() const { return !redoStack.empty(); }

    juce::String getUndoActionName() const
    {
        return canUndo() ? undoStack.back()->getActionName() : "Undo";
    }

    juce::String getRedoActionName() const
    {
        return canRedo() ? redoStack.back()->getActionName() : "Redo";
    }

    void clear()
    {
        undoStack.clear();
        redoStack.clear();
        onStackChanged();
    }

    // Callback when stack changes (for UI updates)
    std::function<void()> onStackChanged = []() {};

private:
    std::vector<std::unique_ptr<UndoableAction>> undoStack, redoStack;
    int maxUndoSteps;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UndoRedoManager)
};

} // namespace VisualCompUndo

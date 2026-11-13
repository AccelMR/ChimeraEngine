/************************************************************************/
/**
 * @file chComponentEditor.cpp
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  What the Inspector shows of a component it has no section of its own for.
 */
/************************************************************************/
#include "chComponentEditor.h"

#if USING(CH_EDITOR)

#include "chAlgorithm.h"

namespace chEngineSDK {

namespace {
/*
 * Pointers, so a description found stays where it is while others are added.
 */
Vector<UniquePtr<ComponentEditorDesc>>&
getDescriptions()
{
  static Vector<UniquePtr<ComponentEditorDesc>> descriptions;
  return descriptions;
}
} // namespace

/*
 */
void
ComponentEditorRegister::add(ComponentEditorDesc&& desc)
{
  for (UniquePtr<ComponentEditorDesc>& existing : getDescriptions()) {
    if (existing->typeId == desc.typeId) {
      *existing = std::move(desc);
      return;
    }
  }
  getDescriptions().push_back(chMakeUnique<ComponentEditorDesc>(std::move(desc)));
}

/*
 */
void
ComponentEditorRegister::remove(const UUID& typeId)
{
  Algorithm::removeFirstIf(getDescriptions(),
                           [&typeId](const UniquePtr<ComponentEditorDesc>& desc) {
                             return desc->typeId == typeId;
                           });
}

/*
 */
const ComponentEditorDesc*
ComponentEditorRegister::find(const UUID& typeId)
{
  for (const UniquePtr<ComponentEditorDesc>& desc : getDescriptions()) {
    if (desc->typeId == typeId) {
      return desc.get();
    }
  }
  return nullptr;
}

/*
 */
const Vector<UniquePtr<ComponentEditorDesc>>&
ComponentEditorRegister::getAll()
{
  return getDescriptions();
}

} // namespace chEngineSDK

#endif // USING(CH_EDITOR)

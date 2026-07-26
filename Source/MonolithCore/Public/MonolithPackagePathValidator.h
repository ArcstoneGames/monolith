#pragma once

#include "CoreMinimal.h"
#include "Misc/PackageName.h"

/**
 * Defensive validator for long package paths (e.g. "/Game/Foo/Bar").
 *
 * Added in response to a fatal editor crash where raw JSON input containing
 * "//Game/..." (double leading slash) was passed directly to CreatePackage().
 * CreatePackage asserts on such inputs in UObjectGlobals.cpp:1012. This
 * wrapper converts the assertion into a recoverable error return.
 *
 * Scope note: routing this validator into CreatePackage call sites is
 * incremental, so grep the Source tree for ValidatePackagePath to see which
 * sites currently own it.
 */
namespace MonolithCore
{
	/**
	 * Validates a long package path.
	 * @param InPath  Package path to check, e.g. "/Game/Foo/Bar".
	 * @return        Empty FString on success; human-readable error message on failure.
	 */
	inline FString ValidatePackagePath(const FString& InPath)
	{
		if (InPath.IsEmpty())
		{
			return TEXT("Package path is empty");
		}

		FText OutReason;
		if (!FPackageName::IsValidLongPackageName(InPath, /*bIncludeReadOnlyRoots=*/false, &OutReason))
		{
			return FString::Printf(TEXT("Invalid package path '%s': %s"), *InPath, *OutReason.ToString());
		}

		return FString();
	}
}

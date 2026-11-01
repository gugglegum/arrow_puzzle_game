# Microsoft Store publication planning

## Advisory snapshot (2026-11-01)

The maintainer reports completing Individual developer registration using a
Russian identity document and Russian address. Their screenshot shows the
Apps and games overview with no products and the new GDK/MSIX game-path banner.
Account verification, product creation, monetization eligibility and actual
payout availability have not been independently checked. This discussion is
publication guidance, not authorization to create or submit a Store product.

### Recommended starting path

Microsoft's current [PC game publishing guidance](https://learn.microsoft.com/en-us/windows/apps/publish/whats-new-game-publishing)
recommends **GDK Game** for existing/new Win32 PC games. It provides a self-service
PC-only path without ID@Xbox enrollment, concept approval or Xbox services.
MSIX or PWA Game is the documented choice for UWP/PWA games. Do not blindly
follow older tutorials that treat all desktop games as ordinary MSIX apps.

ArrowPuzzle is a C++20/raylib Win32 desktop game with OpenGL 3.3 requirements.
The current CMake and packaging helpers produce ordinary/portable executables
and ZIP archives; no Store manifest, MicrosoftGame.config or Store package
workflow was found. Recommend the ordinary user-storage build as the payload;
validate persistence after actual packaged installation. A release ZIP or bare
game EXE is not the GDK submission package. Packaging does not itself establish
compatibility or certification readiness.

### Next steps

1. In Partner Center, select New product > GDK game and reserve the preferred
   name `Arrow Puzzle Game` (reported available by the maintainer). Keep Desktop
   as the initial device family. Retrieve the assigned product identity from
   Game setup before authoring a production package.
2. Install the public GDK and prepare MicrosoftGame.config, shell images,
   executable and required license notices. Build and validate a Store package,
   then test installation, launch, saved games, update and uninstall.
3. Configure Properties (puzzle genre, single player, truthful capabilities,
   support and privacy URL), Age ratings (IARC questionnaire), Store listings
   (initially English/Russian text, current screenshots and required artwork),
   and Pricing and availability (price, markets and visibility).
4. Consider Private audience for a Store-distributed test before public release.
   Finish the certification questionnaire and submit via Review and publish.
   The self-service guide estimates up to three business days for certification,
   plus processing/publishing; this is not a guaranteed deadline.

### Name availability and later renaming (2026-11-01)

The maintainer reports that Partner Center rejects `Arrow Puzzle` as taken
but offers `ArrowPuzzle` as available. They see many similarly named arrow
puzzles in Store search. This is a user-observed availability check, not a
completed reservation or an independently verified trademark clearance.

An unavailable name need not have a public search result: Microsoft documents
names reserved for products not yet submitted; visibility and market settings
can also affect discovery. There is no evidence why other publishers did not
choose the joined spelling, or that their choices establish a search penalty.
Do not claim Store search treats joined/spaced words identically; this was not
documented or tested. Use relevant listing Keywords (for example `arrow puzzle`,
`logic puzzle`, `головоломка со стрелками`) and check real searches after public
indexing. Keywords help discovery but do not guarantee ranking. Avoid copying
competitor titles or stuffing descriptive search phrases into the product name.

The [game-name guide](https://learn.microsoft.com/en-us/gaming/game-publishing/concepts/name)
allows additional names for the same game through Manage product names, including
when changing its name. Select the new reserved Product name in each affected
Store listing and publish the listing update. The installed display name comes
from the package manifest/config and needs a package update for consistency.
Product Identity Details retain their original values; do not create a different
product/identity merely to rename the game. The public name is changeable, but
renaming does not free or grant the currently occupied `Arrow Puzzle` name.
See [Store listing fields](https://learn.microsoft.com/en-us/gaming/game-publishing/concepts/store-listing)
and [Store policy 10.1](https://learn.microsoft.com/en-us/windows/apps/publish/store-policies#101-distinct-function--value-accurate-representation).
The maintainer subsequently checked `Arrow Puzzle Game`, found it available,
and expressed a preference for using it. This supersedes the earlier provisional
`ArrowPuzzle` recommendation. They accept a descriptive name despite similar
games and do not require a distinctive invented brand. Use `Arrow Puzzle Game`
as the preferred Store name in further preparation; actual reservation has not
been reported. No repository/executable/internal identifier rename is requested.

### Packaging references and caveats

- [Create a GDK game](https://learn.microsoft.com/en-us/gaming/game-publishing/tutorial-xbox-managed/how-to-create-a-title-and-configure-game-setup?pivots=msow)
- [Create/upload a package](https://learn.microsoft.com/en-us/gaming/game-publishing/tutorial-xbox-managed/how-to-create-a-package?pivots=msow):
  describes MSIXVC, MakePkg, EKB, symbols and Submission Validator output.
  Without Xbox services, omit MSAAppId, TitleId and ExtendedAttributeList from
  MicrosoftGame.config to avoid a retail Xbox sign-in error.
- [PC packaging](https://learn.microsoft.com/en-us/gaming/gdk/docs/features/common/packaging/overviews/packaging-getting-started-for-pc):
  now also points to MSIXVC2, available with the October 2026 GDK. Original
  MSIXVC remains supported. Select the actual toolchain/upload format during
  implementation; do not confuse these formats with ordinary desktop MSIX.
- [Properties](https://learn.microsoft.com/en-us/gaming/game-publishing/tutorial-xbox-managed/how-to-configure-properties?pivots=msow)
- [Age ratings](https://learn.microsoft.com/en-us/gaming/game-publishing/tutorial-xbox-managed/how-to-set-age-ratings?pivots=msow)
- [Store listing](https://learn.microsoft.com/en-us/gaming/game-publishing/tutorial-xbox-managed/how-to-create-a-store-listing?pivots=msow)
- [Pricing/visibility](https://learn.microsoft.com/en-us/gaming/game-publishing/tutorial-xbox-managed/how-to-configure-pricing-and-availability?pivots=msow)
- [Review and publish](https://learn.microsoft.com/en-us/gaming/game-publishing/tutorial-xbox-managed/how-to-publish-a-base-game?pivots=msow)

Some Learn pages expose both managed-Xbox and Microsoft-Store-on-Windows pivot
content in extracted text. Use the MSOW/self-service instructions; do not infer
that this PC-only route requires managed Xbox contacts or concept approval.

### Monetization remains a separate question

Successful registration does not establish that this particular account can
sell products or receive funds. The current [account guidance](https://learn.microsoft.com/en-us/windows/apps/publish/partner-center/open-a-developer-account)
describes Individual for hobby/non-commercial publishing and Company for
business/professional publishing; it says conversion between these types is
unsupported. If paid distribution is desired, clarify eligibility with Store
support before committing to a commercial account/product setup.

The [regional payout table](https://learn.microsoft.com/en-us/partner-center/marketplace-offers/payment-thresholds-methods-timeframes)
lists Russia as supported for Microsoft Store but PayPal as suspended. This
table is not proof that a particular Russian bank/account can receive money.
Check the actual Payout and tax profile and use
[Windows developer support](https://developer.microsoft.com/en-us/windows/support/)
for account-specific payment questions. Do not promise payments or claim all
Russian publication is prohibited based only on historical restrictions.

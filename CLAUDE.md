# プロジェクトルール

## プロジェクト概要
- 2.5Dゲーム（リトルナイトメアのようなカメラ挙動）3/10の割合
- マリオギャラクシーの2.5D版のようなゲームを作る予定 7/10の割合で
- マリオギャラクシーのような重力（惑星間の移動など）
- カメラがめっちゃ重要
- マップ各所に仲間種族『コアリア』NPCを配置し、話しかけるとヒントを表示するチュートリアル導線を実装する。

## コーディング規約

### 当たり判定
- AABBは絶対使わない(Axis-Aligned Bounding Box)
- 判定はすべてレイキャストで行うこと
- Box惑星・壁・床などあらゆる当たり判定でもAABBによる距離・範囲チェックは禁止
- 範囲内かどうかの判定も `std::abs(lp.x) > half.x` のようなAABB式は使わず、レイキャスト結果（hitNDir・hitPos・overlapDistance）のみを使うこと
- `KdBoxCollision` の当たり判定はレイキャスト（`Intersects(RayInfo)`）を使うこと

### メモリ管理
- 生ポインタ（raw pointer）は使用禁止
- スマートポインター（`std::unique_ptr`、`std::shared_ptr`、`std::weak_ptr`）を必ず使用すること

### 定数・マジックナンバー
- マジックナンバーの直書き禁止（ローカル変数への直値代入も含む）
- 数値定数は `constexpr` または `const` で名前付き定数として定義すること
- 定数はカテゴリごとに専用のヘッダーファイルにまとめること
  - 命名規則: `○○Const.h`（例: `UIConst.h`、`PlayerConst.h`、`EnemyConst.h`）
  - UI関連の定数は `UIConst.h` に記述すること
  - プレイヤー関連の定数は `PlayerConst.h` に記述すること
  - 敵関連の定数は `EnemyConst.h` に記述すること
  - 各システム・機能ごとに対応する `○○Const.h` を作成すること

### 座標・ベクトル
- XYZ座標系の値は `Vector3` 型または専用の構造体・列挙体を使用すること
- 生の `float x, y, z` をバラバラに扱うことは避け、まとめて構造体として扱うこと

### 型・データ表現
- 状態や種別の表現には列挙体（`enum class`）を使用すること
- 座標・方向・スケールなどは `Vector3` や専用構造体でまとめること
- オブジェクト系はobjectlistやobjectmanagerなどの管理クラスを作成して管理すること（`std::list<std::shared_ptr<KdGameObject>> m_objList;`）
